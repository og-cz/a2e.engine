#include "a2e/physics.hpp"

#include "a2e/entity.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace a2e {
namespace {

bool overlaps(const Entity& first, const Collider& first_collider,
              const Entity& second, const Collider& second_collider) {
    const auto& first_transform = first.transform();
    const auto& second_transform = second.transform();
    const double first_half_width = first_collider.width * first_transform.scale_x / 2.0;
    const double first_half_height = first_collider.height * first_transform.scale_y / 2.0;
    const double second_half_width = second_collider.width * second_transform.scale_x / 2.0;
    const double second_half_height = second_collider.height * second_transform.scale_y / 2.0;
    return first_transform.x - first_half_width < second_transform.x + second_half_width &&
           first_transform.x + first_half_width > second_transform.x - second_half_width &&
           first_transform.y - first_half_height < second_transform.y + second_half_height &&
           first_transform.y + first_half_height > second_transform.y - second_half_height;
}

void resolve_solid_overlap(Entity& first, const Collider& first_collider,
                           Entity& second, const Collider& second_collider) {
    auto* first_body = first.rigid_body();
    auto* second_body = second.rigid_body();
    const bool first_dynamic = first_body && first_body->dynamic;
    const bool second_dynamic = second_body && second_body->dynamic;
    if (!first_dynamic && !second_dynamic) return;

    const auto& first_transform = first.transform();
    const auto& second_transform = second.transform();
    const double first_half_width = first_collider.width * first_transform.scale_x / 2.0;
    const double first_half_height = first_collider.height * first_transform.scale_y / 2.0;
    const double second_half_width = second_collider.width * second_transform.scale_x / 2.0;
    const double second_half_height = second_collider.height * second_transform.scale_y / 2.0;
    const double horizontal_penetration = first_half_width + second_half_width -
                                          std::abs(first_transform.x - second_transform.x);
    const double vertical_penetration = first_half_height + second_half_height -
                                        std::abs(first_transform.y - second_transform.y);
    const bool horizontal = horizontal_penetration < vertical_penetration;
    const double penetration = horizontal ? horizontal_penetration : vertical_penetration;
    const double direction = horizontal
                                 ? (second_transform.x >= first_transform.x ? 1.0 : -1.0)
                                 : (second_transform.y >= first_transform.y ? 1.0 : -1.0);
    const double restitution = first_dynamic && second_dynamic
                                   ? std::max(first_body->restitution, second_body->restitution)
                                   : first_dynamic ? first_body->restitution : second_body->restitution;
    const double friction = first_dynamic && second_dynamic
                                ? std::clamp((first_body->friction + second_body->friction) / 2.0, 0.0, 1.0)
                                : first_dynamic ? std::clamp(first_body->friction, 0.0, 1.0)
                                                 : std::clamp(second_body->friction, 0.0, 1.0);

    auto apply_response = [horizontal, restitution, friction,
                           &first_transform, &second_transform](RigidBody& body, bool first_body_side) {
        double& normal_velocity = horizontal ? body.velocity_x : body.velocity_y;
        double& tangent_velocity = horizontal ? body.velocity_y : body.velocity_x;
        const double position_delta = horizontal
                                          ? (first_body_side ? first_transform.x - second_transform.x
                                                             : second_transform.x - first_transform.x)
                                          : (first_body_side ? first_transform.y - second_transform.y
                                                             : second_transform.y - first_transform.y);
        if (restitution == 0.0 || position_delta * normal_velocity < 0.0) {
            normal_velocity = -normal_velocity * restitution;
        }
        tangent_velocity *= (1.0 - friction);
    };

    if (first_dynamic && second_dynamic) {
        if (horizontal) {
            first.transform().x -= direction * penetration / 2.0;
            second.transform().x += direction * penetration / 2.0;
        } else {
            first.transform().y -= direction * penetration / 2.0;
            second.transform().y += direction * penetration / 2.0;
        }
        apply_response(*first_body, true);
        apply_response(*second_body, false);
    } else if (first_dynamic) {
        if (horizontal) {
            first.transform().x -= direction * penetration;
        } else {
            first.transform().y -= direction * penetration;
        }
        apply_response(*first_body, true);
    } else {
        if (horizontal) {
            second.transform().x += direction * penetration;
        } else {
            second.transform().y += direction * penetration;
        }
        apply_response(*second_body, false);
    }
}

} // namespace

PhysicsSystem::PhysicsSystem(CollisionCallback callback, double gravity_y, EventBus* events,
                             double max_step_seconds)
    : callback_(std::move(callback)), gravity_y_(gravity_y), events_(events),
            max_step_seconds_(max_step_seconds) {
        if (max_step_seconds < 0.0) throw std::invalid_argument("maximum physics step cannot be negative");
}

void PhysicsSystem::update(Scene& scene, const InputState&, double delta_seconds) {
    if (max_step_seconds_ <= 0.0 || delta_seconds <= max_step_seconds_) {
        update_step(scene, delta_seconds);
        return;
    }

    double remaining = delta_seconds;
    while (remaining > 0.0) {
        const double step = std::min(remaining, max_step_seconds_);
        update_step(scene, step);
        remaining -= step;
    }
}

void PhysicsSystem::update_step(Scene& scene, double delta_seconds) {
    for (const auto& entity : scene.entities()) {
        if (!entity->active()) continue;
        auto* body = entity->rigid_body();
        if (body && body->dynamic) {
            body->velocity_y += gravity_y_ * delta_seconds;
            entity->transform().x += body->velocity_x * delta_seconds;
            entity->transform().y += body->velocity_y * delta_seconds;
        }
    }

    std::map<std::pair<std::uint64_t, std::uint64_t>, bool> current_contacts;
    for (std::size_t first_index = 0; first_index < scene.entities().size(); ++first_index) {
        const auto& first = scene.entities()[first_index];
        if (!first->active()) continue;
        const auto* first_collider = first->collider();
        if (!first_collider) continue;
        for (std::size_t second_index = first_index + 1; second_index < scene.entities().size(); ++second_index) {
            const auto& second = scene.entities()[second_index];
            if (!second->active()) continue;
            const auto* second_collider = second->collider();
            if (!second_collider) continue;
            if ((first_collider->layer & second_collider->mask) == 0 ||
                (second_collider->layer & first_collider->mask) == 0 ||
                !overlaps(*first, *first_collider, *second, *second_collider)) {
                continue;
            }
            const bool trigger = first_collider->trigger || second_collider->trigger;
            const auto& first_transform = first->transform();
            const auto& second_transform = second->transform();
            const double horizontal_penetration =
                first_collider->width * first_transform.scale_x / 2.0 +
                second_collider->width * second_transform.scale_x / 2.0 -
                std::abs(first_transform.x - second_transform.x);
            const double vertical_penetration =
                first_collider->height * first_transform.scale_y / 2.0 +
                second_collider->height * second_transform.scale_y / 2.0 -
                std::abs(first_transform.y - second_transform.y);
            const bool horizontal = horizontal_penetration < vertical_penetration;
            const double direction = horizontal
                                         ? (second_transform.x >= first_transform.x ? 1.0 : -1.0)
                                         : (second_transform.y >= first_transform.y ? 1.0 : -1.0);
            const double normal_x = horizontal ? direction : 0.0;
            const double normal_y = horizontal ? 0.0 : direction;
            const double penetration = horizontal ? horizontal_penetration : vertical_penetration;
            const auto contact = std::make_pair(first->id(), second->id());
            current_contacts[contact] = trigger;
            if (callback_) callback_({first->id(), second->id(), trigger, normal_x, normal_y, penetration});
            if (events_ && active_contacts_.find(contact) == active_contacts_.end()) {
                events_->publish(CollisionEnterEvent{first->id(), second->id(), trigger,
                                                     normal_x, normal_y, penetration});
            }
            if (!trigger) resolve_solid_overlap(*first, *first_collider, *second, *second_collider);
        }
    }

    if (events_) {
        for (const auto& [contact, trigger] : active_contacts_) {
            if (current_contacts.find(contact) == current_contacts.end()) {
                events_->publish(CollisionExitEvent{contact.first, contact.second, trigger});
            }
        }
    }
    active_contacts_ = std::move(current_contacts);
}

} // namespace a2e
