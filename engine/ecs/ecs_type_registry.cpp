// =============================================================================
// Explicit entt::type_index instantiations for cross-DLL ECS.
//
// engine_core (ENTT_API_EXPORT) must export type_index<T>::value() for every
// component type used by other DLLs (engine_client, engine_voxel, engine_app).
// Without these, consumer DLLs compiled with ENTT_API_IMPORT get LNK2019.
//
// Forward declarations suffice — type_index<T>::value() does not inspect T.
// =============================================================================

#include <entt/entt.hpp>

// Forward-declare ALL component types to avoid pulling in glm/glad headers
// that engine_core doesn't link. type_index<T> only needs a forward declaration.
namespace ecs {
// components.hpp
struct Transform;
struct Velocity;
struct PreviousPosition;
struct BoxCollider;
struct PlayerTag;
struct PlayerController;
struct FirstPersonCamera;
struct GravityAffected;
struct RigidBody;
struct MeshComponent;
struct ModelComponent;
struct ToolHolder;
struct BlockBreaker;
struct InputState;
struct NameTag;
struct Lifetime;
struct Active;

// components/rendering.hpp
struct Sprite;
struct AnimatedSprite;
struct AnimationSet;
struct Camera2DController;
struct CameraBounds;
struct CameraTarget;
struct Particle;
struct ParticleEmitter;
struct FlashEffect;
struct TrailEffect;
struct HealthBar;
struct WorldLabel;
struct RenderLayer;

// components/common.hpp
struct Health;
struct DamageMultiplier;
struct Invulnerable;
struct Weapon;
struct WeaponState;
struct AIController;
struct AITarget;
struct PatrolPath;
struct Transform2D;
struct Velocity2D;
struct Acceleration2D;
struct Movement2D;
struct CircleCollider;
struct BoxCollider2D;
struct CollisionLayer;
struct EnemyTag;
struct ProjectileTag;
struct DeadTag;
struct Team;
}

// ---------------------------------------------------------------------------
// components.hpp
// ---------------------------------------------------------------------------
template struct entt::type_index<ecs::Transform, void>;
template struct entt::type_index<ecs::Velocity, void>;
template struct entt::type_index<ecs::PreviousPosition, void>;
template struct entt::type_index<ecs::BoxCollider, void>;
template struct entt::type_index<ecs::PlayerTag, void>;
template struct entt::type_index<ecs::PlayerController, void>;
template struct entt::type_index<ecs::FirstPersonCamera, void>;
template struct entt::type_index<ecs::GravityAffected, void>;
template struct entt::type_index<ecs::RigidBody, void>;
template struct entt::type_index<ecs::MeshComponent, void>;
template struct entt::type_index<ecs::ModelComponent, void>;
template struct entt::type_index<ecs::ToolHolder, void>;
template struct entt::type_index<ecs::BlockBreaker, void>;
template struct entt::type_index<ecs::InputState, void>;
template struct entt::type_index<ecs::NameTag, void>;
template struct entt::type_index<ecs::Lifetime, void>;
template struct entt::type_index<ecs::Active, void>;

// ---------------------------------------------------------------------------
// components/rendering.hpp  (forward-declared above)
// ---------------------------------------------------------------------------
template struct entt::type_index<ecs::Sprite, void>;
template struct entt::type_index<ecs::AnimatedSprite, void>;
template struct entt::type_index<ecs::AnimationSet, void>;
template struct entt::type_index<ecs::Camera2DController, void>;
template struct entt::type_index<ecs::CameraBounds, void>;
template struct entt::type_index<ecs::CameraTarget, void>;
template struct entt::type_index<ecs::Particle, void>;
template struct entt::type_index<ecs::ParticleEmitter, void>;
template struct entt::type_index<ecs::FlashEffect, void>;
template struct entt::type_index<ecs::TrailEffect, void>;
template struct entt::type_index<ecs::HealthBar, void>;
template struct entt::type_index<ecs::WorldLabel, void>;
template struct entt::type_index<ecs::RenderLayer, void>;

// ---------------------------------------------------------------------------
// components/common.hpp
// ---------------------------------------------------------------------------
template struct entt::type_index<ecs::Health, void>;
template struct entt::type_index<ecs::DamageMultiplier, void>;
template struct entt::type_index<ecs::Invulnerable, void>;
template struct entt::type_index<ecs::Weapon, void>;
template struct entt::type_index<ecs::WeaponState, void>;
template struct entt::type_index<ecs::AIController, void>;
template struct entt::type_index<ecs::AITarget, void>;
template struct entt::type_index<ecs::PatrolPath, void>;
template struct entt::type_index<ecs::Transform2D, void>;
template struct entt::type_index<ecs::Velocity2D, void>;
template struct entt::type_index<ecs::Acceleration2D, void>;
template struct entt::type_index<ecs::Movement2D, void>;
template struct entt::type_index<ecs::CircleCollider, void>;
template struct entt::type_index<ecs::BoxCollider2D, void>;
template struct entt::type_index<ecs::CollisionLayer, void>;
template struct entt::type_index<ecs::EnemyTag, void>;
template struct entt::type_index<ecs::ProjectileTag, void>;
template struct entt::type_index<ecs::DeadTag, void>;
template struct entt::type_index<ecs::Team, void>;
