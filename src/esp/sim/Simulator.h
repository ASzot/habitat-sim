// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef ESP_SIM_SIMULATOR_H_
#define ESP_SIM_SIMULATOR_H_

#include "esp/agent/Agent.h"
#include "esp/assets/ResourceManager.h"
#include "esp/assets/managers/AssetAttributesManager.h"
#include "esp/assets/managers/ObjectAttributesManager.h"
#include "esp/assets/managers/PhysicsAttributesManager.h"
#include "esp/assets/managers/StageAttributesManager.h"
#include "esp/core/esp.h"
#include "esp/core/random.h"
#include "esp/gfx/RenderTarget.h"
#include "esp/gfx/WindowlessContext.h"
#include "esp/nav/PathFinder.h"
#include "esp/physics/ArticulatedObject.h"
#include "esp/physics/PhysicsManager.h"
#include "esp/physics/RigidObject.h"
#include "esp/scene/SceneConfiguration.h"
#include "esp/scene/SceneManager.h"
#include "esp/scene/SceneNode.h"

namespace esp {
namespace nav {
class PathFinder;
class NavMeshSettings;
class ActionSpacePathFinder;
}  // namespace nav
namespace scene {
class SemanticScene;
}  // namespace scene
namespace gfx {
class Renderer;
}  // namespace gfx
}  // namespace esp

namespace esp {
namespace sim {

namespace AttrMgrs = esp::assets::managers;
namespace Attrs = esp::assets::attributes;

struct SimulatorConfiguration {
  scene::SceneConfiguration scene;
  int defaultAgentId = 0;
  int gpuDeviceId = 0;
  unsigned int randomSeed = 0;
  std::string defaultCameraUuid = "rgba_camera";
  bool compressTextures = false;
  bool createRenderer = true;
  // Whether or not the agent can slide on collisions
  bool allowSliding = true;
  // enable or disable the frustum culling
  bool frustumCulling = true;
  /**
   * @brief This flags specifies whether or not dynamics is supported by the
   * simulation, if a suitable library (i.e. Bullet) has been installed.
   */
  bool enablePhysics = false;
  bool loadSemanticMesh = true;
  std::string physicsConfigFile =
      ESP_DEFAULT_PHYS_SCENE_CONFIG_REL_PATH;  // should we instead link a
                                               // PhysicsManagerConfiguration
                                               // object here?
  /** @brief Light setup key for scene */
  std::string sceneLightSetup = assets::ResourceManager::NO_LIGHT_KEY;

  ESP_SMART_POINTERS(SimulatorConfiguration)
};
bool operator==(const SimulatorConfiguration& a,
                const SimulatorConfiguration& b);
bool operator!=(const SimulatorConfiguration& a,
                const SimulatorConfiguration& b);

class Simulator {
 public:
  explicit Simulator(const SimulatorConfiguration& cfg);
  virtual ~Simulator();

  /**
   * @brief Closes the simulator and frees all loaded assets and GPU contexts.
   *
   * @warning Must reset the simulator to its "just after constructor" state for
   * python inheritance to function correctly.  Shared/unique pointers should be
   * set back to nullptr, any members set to their default values, etc.  If this
   * is not done correctly, the pattern for @ref `close` then @ref `reconfigure`
   * to create a "fresh" instance of the simulator may not work correctly
   */
  virtual void close();

  virtual void reconfigure(const SimulatorConfiguration& cfg);

  virtual void reset();

 public:
  virtual void seed(uint32_t newSeed);

  std::shared_ptr<gfx::Renderer> getRenderer() { return renderer_; }
  std::shared_ptr<scene::SemanticScene> getSemanticScene() {
    return semanticScene_;
  }

  scene::SceneGraph& getActiveSceneGraph();
  scene::SceneGraph& getActiveSemanticSceneGraph();

  void saveFrame(const std::string& filename);

  /**
   * @brief The ID of the CUDA device of the OpenGL context owned by the
   * simulator.  This will only be nonzero if the simulator is built in
   * --headless mode on linux
   */
  int gpuDevice() const { return context_->gpuDevice(); }

  // === Physics Simulator Functions ===
  // TODO: support multi-scene physics (default sceneID=0 currently).

  /**
   * @brief Return manager for construction and access to asset attributes.
   */
  const AttrMgrs::AssetAttributesManager::ptr getAssetAttributesManager()
      const {
    return resourceManager_->getAssetAttributesManager();
  }
  /**
   * @brief Return manager for construction and access to object attributes.
   */
  const AttrMgrs::ObjectAttributesManager::ptr getObjectAttributesManager()
      const {
    return resourceManager_->getObjectAttributesManager();
  }
  /**
   * @brief Return manager for construction and access to physics world
   * attributes.
   */
  const AttrMgrs::PhysicsAttributesManager::ptr getPhysicsAttributesManager()
      const {
    return resourceManager_->getPhysicsAttributesManager();
  }
  /**
   * @brief Return manager for construction and access to scene attributes.
   */
  const AttrMgrs::StageAttributesManager::ptr getStageAttributesManager()
      const {
    return resourceManager_->getStageAttributesManager();
  }

  /** @brief Return the library implementation type for the simulator currently
   * in use. Use to check for a particular implementation.
   * @return The implementation type of this simulator.
   */
  const esp::physics::PhysicsManager::PhysicsSimulationLibrary&
  getPhysicsSimulationLibrary() const {
    return physicsManager_->getPhysicsSimulationLibrary();
  };

  /**
   * @brief Instance an object from a template ID in @ref
   * esp::managers::ObjectAttributesManager. See @ref
   * esp::physics::PhysicsManager::addObject().
   * @param objectLibId The ID of the object's template in @ref
   * esp::managers::ObjectAttributesManager.
   * @param attachmentNode If provided, attach the RigidObject Feature to this
   * node instead of creating a new one.
   * @param lightSetupKey The string key for the @ref gfx::LightSetup to be used
   * by this object.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * add an object to.
   * @return The ID assigned to new object which identifies it in @ref
   * esp::physics::PhysicsManager::existingObjects_ or @ref esp::ID_UNDEFINED if
   * instancing fails.
   */
  int addObject(int objectLibId,
                scene::SceneNode* attachmentNode = nullptr,
                const std::string& lightSetupKey =
                    assets::ResourceManager::DEFAULT_LIGHTING_KEY,
                int sceneID = 0);

  /**
   * @brief Instance an object from a template ID in @ref
   * esp::managers::ObjectAttributesManager. See @ref
   * esp::physics::PhysicsManager::addObject().
   * @param objectLibHandle The handle of the object's template in
   * @ref esp::managers::ObjectAttributesManager.
   * @param attachmentNode If provided, attach the RigidObject Feature to this
   * node instead of creating a new one.
   * @param lightSetupKey The string key for the @ref gfx::LightSetup to be used
   * by this object.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * add an object to.
   * @return The ID assigned to new object which identifies it in @ref
   * esp::physics::PhysicsManager::existingObjects_ or @ref esp::ID_UNDEFINED if
   * instancing fails.
   */
  int addObjectByHandle(const std::string& objectLibHandle,
                        scene::SceneNode* attachmentNode = nullptr,
                        const std::string& lightSetupKey =
                            assets::ResourceManager::DEFAULT_LIGHTING_KEY,
                        int sceneID = 0);

  /**
   * @brief Get a static view of a physics object's template when the object was
   * instanced.
   *
   * Use this to query the object's properties when it was initialized.  Object
   * pointed at by pointer is const, and can not be modified.
   */
  const Attrs::ObjectAttributes::cptr getObjectInitializationTemplate(
      int objectId,
      const int sceneID = 0) const;

  /**
   * @brief Remove an instanced object by ID. See @ref
   * esp::physics::PhysicsManager::removeObject().
   * @param objectID The ID of the object identifying it in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * remove the object from.
   */
  void removeObject(const int objectID,
                    bool deleteObjectNode = true,
                    bool deleteVisualNode = true,
                    const int sceneID = 0);

  /**
   * @brief Get the IDs of the physics objects instanced in a physical scene.
   * See @ref esp::physics::PhysicsManager::getExistingObjectIDs.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * query.
   * @return A vector of ID keys into @ref
   * esp::physics::PhysicsManager::existingObjects_.
   */
  std::vector<int> getExistingObjectIDs(const int sceneID = 0);

  /**
   * @brief Get the @ref esp::physics::MotionType of an object.
   * See @ref esp::physics::PhysicsManager::getExistingObjectIDs.
   * @param objectID The ID of the object identifying it in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * query.
   * @return The @ref esp::physics::MotionType of the object or @ref
   * esp::physics::MotionType::ERROR_MOTIONTYPE if query failed.
   */
  esp::physics::MotionType getObjectMotionType(const int objectID,
                                               const int sceneID = 0);

  /**
   * @brief Set the @ref esp::physics::MotionType of an object.
   * See @ref esp::physics::PhysicsManager::getExistingObjectIDs.
   * @param motionType The desired motion type of the object
   * @param objectID The ID of the object identifying it in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene to
   * query.
   * @return whether or not the set was successful.
   */
  bool setObjectMotionType(const esp::physics::MotionType& motionType,
                           const int objectID,
                           const int sceneID = 0);

  /**@brief Retrieves a shared pointer to the VelocityControl struct for this
   * object.
   */
  physics::VelocityControl::ptr getObjectVelocityControl(
      const int objectID,
      const int sceneID = 0) const;

  /**
   * @brief Apply torque to an object. See @ref
   * esp::physics::PhysicsManager::applyTorque.
   * @param tau The desired torque to apply.
   * @param objectID The ID of the object identifying it in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void applyTorque(const Magnum::Vector3& tau,
                   const int objectID,
                   const int sceneID = 0);

  /**
   * @brief Apply force to an object. See @ref
   * esp::physics::PhysicsManager::applyForce.
   * @param force The desired linear force to apply.
   * @param relPos The desired location relative to the object origin at which
   * to apply the force.
   * @param objectID The ID of the object identifying it in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void applyForce(const Magnum::Vector3& force,
                  const Magnum::Vector3& relPos,
                  const int objectID,
                  const int sceneID = 0);

  /**
   * @brief Get a reference to the object's scene node or nullptr if failed.
   */
  scene::SceneNode* getObjectSceneNode(const int objectID,
                                       const int sceneID = 0);

  /**
   * @brief Get references to the object's visual scene nodes or empty if
   * failed.
   */
  std::vector<scene::SceneNode*> getObjectVisualSceneNodes(
      const int objectID,
      const int sceneID = 0);

  /**
   * @brief Get the current 4x4 transformation matrix of an object.
   * See @ref esp::physics::PhysicsManager::getTransformation.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return The 4x4 transform of the object.
   */
  Magnum::Matrix4 getTransformation(const int objectID, const int sceneID = 0);

  /**
   * @brief Set the 4x4 transformation matrix of an object kinematically.
   * See @ref esp::physics::PhysicsManager::setTransformation.
   * @param transform The desired 4x4 transform of the object.
   * @param  objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setTransformation(const Magnum::Matrix4& transform,
                         const int objectID,
                         const int sceneID = 0);
  /**
   * @brief Get the current @ref esp::core::RigidState of an object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return The @ref esp::core::RigidState transform of the object.
   */
  esp::core::RigidState getRigidState(const int objectID,
                                      const int sceneID = 0) const;

  /**
   * @brief Set the @ref esp::core::RigidState of an object kinematically.
   * @param transform The desired @ref esp::core::RigidState of the object.
   * @param  objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setRigidState(const esp::core::RigidState& rigidState,
                     const int objectID,
                     const int sceneID = 0);

  /**
   * @brief Set the 3D position of an object kinematically.
   * See @ref esp::physics::PhysicsManager::setTranslation.
   * @param translation The desired 3D position of the object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setTranslation(const Magnum::Vector3& translation,
                      const int objectID,
                      const int sceneID = 0);

  /**
   * @brief Get the current 3D position of an object.
   * See @ref esp::physics::PhysicsManager::getTranslation.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return The 3D position of the object.
   */
  Magnum::Vector3 getTranslation(const int objectID, const int sceneID = 0);

  /**
   * @brief Set the orientation of an object kinematically.
   * See @ref esp::physics::PhysicsManager::setRotation.
   * @param rotation The desired orientation of the object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setRotation(const Magnum::Quaternion& rotation,
                   const int objectID,
                   const int sceneID = 0);

  /**
   * @brief Get the current orientation of an object.
   * See @ref esp::physics::PhysicsManager::getRotation.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return A quaternion representation of the object's orientation.
   */
  Magnum::Quaternion getRotation(const int objectID, const int sceneID = 0);

  /**
   * @brief Set the Linear Velocity of object.
   * See @ref esp::physics::PhysicsManager::setLinearVelocity.
   * @param linVel The desired linear velocity of the object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setLinearVelocity(const Magnum::Vector3& linVel,
                         const int objectID,
                         const int sceneID = 0);

  /**
   * @brief Get the Linear Velocity of object.
   * See @ref esp::physics::PhysicsManager::getLinearVelocity.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return A vector3 representation of the object's linear velocity.
   */
  Magnum::Vector3 getLinearVelocity(const int objectID, const int sceneID = 0);

  /**
   * @brief Set the Angular Velocity of object.
   * See @ref esp::physics::PhysicsManager::setAngularVelocity.
   * @param angVel The desired angular velocity of the object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setAngularVelocity(const Magnum::Vector3& angVel,
                          const int objectID,
                          const int sceneID = 0);

  /**
   * @brief Get the Angular Velocity of object.
   * See @ref esp::physics::PhysicsManager::getAngularVelocity.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return A vector3 representation of the object's angular velocity.
   */
  Magnum::Vector3 getAngularVelocity(const int objectID, const int sceneID = 0);

  /**
   * @brief Turn on/off rendering for the bounding box of the object's visual
   * component.
   *
   * Assumes the new @ref esp::gfx::Drawable for the bounding box should be
   * added to the active @ref esp::gfx::SceneGraph's default drawable group. See
   * @ref esp::gfx::SceneGraph::getDrawables().
   *
   * @param drawBB Whether or not the render the bounding box.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setObjectBBDraw(bool drawBB, const int objectID, const int sceneID = 0);

  //===============================================================================//
  // Articulated Object API (UNSTABLE!)

  /**
   * @brief Parse a URDF and instantiate the defined robot if successful.
   *
   * This will always re-parse the URDF. Edits to URDF parameters between calls
   * will be reflected. Edits to linked and imported assets between calls will
   * NOT be reflected (e.g. modifying the geometry of a part between loads).
   */
  int addArticulatedObjectFromURDF(std::string filepath,
                                   bool fixedBase = false);

  void removeArticulatedObject(int objectId);

  /** @brief Get a list of existing object IDs for articulated objects (i.e.,
   * existing keys in @ref PhysicsManager::existingArticulatedObjects_.)
   */
  std::vector<int> getExistingArticulatedObjectIDs(int sceneID = 0);

  void setArticulatedObjectRootState(int objectId,
                                     const Magnum::Matrix4& state);

  const Magnum::Matrix4 getArticulatedObjectRootState(int objectId);

  void setArticulatedObjectForces(int objectId, std::vector<float> forces);

  void setArticulatedObjectVelocities(int objectId, std::vector<float> vels);

  void setArticulatedObjectPositions(int objectId,
                                     std::vector<float> positions);

  std::vector<float> getArticulatedObjectPositions(int objectId);

  std::vector<float> getArticulatedObjectVelocities(int objectId);

  std::vector<float> getArticulatedObjectForces(int objectId);

  void resetArticulatedObject(int objectId);

  void setArticulatedObjectSleep(int objectId, bool sleep);

  bool getArticulatedObjectSleep(int objectId);

  void setArticulatedObjectMotionType(int objectId,
                                      esp::physics::MotionType mt);

  esp::physics::MotionType getArticulatedObjectMotionType(int objectId);

  int getNumArticulatedLinks(int objectId);
  core::RigidState getArticulatedLinkRigidState(int objectId, int linkId);

  // Joint Motor API

  /**
   * @brief Create a new JointMotor for a dof in an ArticulatedObject from a
   * JointMotorSettings.
   *
   * @return The motorId for the new joint motor or ID_UNDEFINED (-1) if failed.
   */
  int createJointMotor(const int objectId,
                       const int dof,
                       const esp::physics::JointMotorSettings& settings);

  /**
   * @brief Remove and destroy a JointMotor for an ArticulatedObject.
   */
  void removeJointMotor(const int objectId, const int motorId);

  /**
   * @brief Get a copy of the JointMotorSettings for an ArticulatedObject's
   * existing JointMotor .
   */
  esp::physics::JointMotorSettings getJointMotorSettings(const int objectId,
                                                         const int motorId);

  /**
   * @brief Update an ArticulatedObject's JointMotor with new settings.
   */
  void updateJointMotor(const int objectId,
                        const int motorId,
                        const esp::physics::JointMotorSettings& settings);

  /**
   * @brief Query a map of motorIds -> dofs for all active JointMotors attached
   * to an ArticulatedObject.
   */
  std::map<int, int> getExistingJointMotors(const int objectId);

  /**
   * @brief Create a new set of default JointMotors for all valid dofs in an
   * ArticulatedObject.
   *
   * Note: No base implementation. See @ref bullet::BulletArticulatedObject.
   *
   * @return A map of dofs -> motorIds for the new motors.
   */
  std::map<int, int> createMotorsForAllDofs(
      const int objectId,
      esp::physics::JointMotorSettings settings =
          esp::physics::JointMotorSettings());

  // END: Articulated Object API (UNSTABLE!)
  //===============================================================================//

  /**
   * @brief Set the @ref esp::scene:SceneNode::semanticId_ for all visual nodes
   * belonging to an object.
   *
   * @param semanticId The desired semantic id for the object.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   */
  void setObjectSemanticId(uint32_t semanticId, int objectID, int sceneID = 0);

  /**
   * @brief Discrete collision check for contact between an object and the
   * collision world.
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return Whether or not the object is in contact with any other collision
   * enabled objects.
   */
  bool contactTest(const int objectID, const int sceneID = 0);

  std::vector<esp::physics::ContactPointData> getPhysicsContactPoints(
      const int sceneID);

  /**
   * @brief Raycast into the collision world of a scene.
   *
   * Note: A default @ref physics::PhysicsManager has no collision world, so
   * physics must be enabled for this feature.
   *
   * @param ray The ray to cast. Need not be unit length, but returned hit
   * distances will be in units of ray length.
   * @param maxDistance The maximum distance along the ray direction to search.
   * In units of ray length.
   * @param sceneID !! Not used currently !! Specifies which physical scene of
   * the object.
   * @return Raycast results sorted by distance.
   */
  esp::physics::RaycastResults castRay(const esp::geo::Ray& ray,
                                       float maxDistance = 100.0,
                                       const int sceneID = 0);

  /**
   * @brief the physical world has a notion of time which passes during
   * animation/simulation/action/etc... Step the physical world forward in time
   * by a desired duration. Note that the actual duration of time passed by this
   * step will depend on simulation time stepping mode (todo). See @ref
   * esp::physics::PhysicsManager::stepPhysics.
   * @todo timestepping options?
   * @param dt The desired amount of time to advance the physical world.
   * @return The new world time after stepping. See @ref
   * esp::physics::PhysicsManager::worldTime_.
   */
  double stepWorld(const double dt = 1.0 / 60.0);

  /**
   * @brief Get the current time in the simulated world. This is always 0 if no
   * @ref esp::physics::PhysicsManager is initialized. See @ref stepWorld. See
   * @ref esp::physics::PhysicsManager::getWorldTime.
   * @return The amount of time, @ref esp::physics::PhysicsManager::worldTime_,
   * by which the physical world has advanced.
   */
  double getWorldTime();

  /**
   * @brief Set the gravity in a physical scene.
   */
  void setGravity(const Magnum::Vector3& gravity, const int sceneID = 0);

  /**
   * @brief Get the gravity in a physical scene.
   */
  Magnum::Vector3 getGravity(const int sceneID = 0) const;

  /**
   * @brief Compute the navmesh for the simulator's current active scene and
   * assign it to the referenced @ref nav::PathFinder.
   * @param pathfinder The pathfinder object to which the recomputed navmesh
   * will be assigned.
   * @param navMeshSettings The @ref nav::NavMeshSettings instance to
   * parameterize the navmesh construction.
   * @return Whether or not the navmesh recomputation succeeded.
   */
  bool recomputeNavMesh(nav::PathFinder& pathfinder,
                        const nav::NavMeshSettings& navMeshSettings,
                        bool includeStaticObjects = false);

  /**
   * @brief Set visualization of the current NavMesh @ref pathfinder_ on or off.
   *
   * @param visualize Whether or not to visualize the navmesh.
   * @return Whether or not the NavMesh visualization is active.
   */
  bool setNavMeshVisualization(bool visualize);

  /**
   * @brief Query active state of the current NavMesh @ref pathfinder_
   * visualization.
   */
  bool isNavMeshVisualizationActive();

  agent::Agent::ptr getAgent(int agentId);

  agent::Agent::ptr addAgent(const agent::AgentConfiguration& agentConfig,
                             scene::SceneNode& agentParentNode);
  agent::Agent::ptr addAgent(const agent::AgentConfiguration& agentConfig);

  /**
   * @brief Displays observations on default frame buffer for a
   * particular sensor of an agent
   * @param agentId    Id of the agent for which the observation is to
   *                   be returned
   * @param sensorId   Id of the sensor for which the observation is to
   *                   be returned
   */
  bool displayObservation(int agentId, const std::string& sensorId);
  bool getAgentObservation(int agentId,
                           const std::string& sensorId,
                           sensor::Observation& observation);
  int getAgentObservations(
      int agentId,
      std::map<std::string, sensor::Observation>& observations);

  bool getAgentObservationSpace(int agentId,
                                const std::string& sensorId,
                                sensor::ObservationSpace& space);
  int getAgentObservationSpaces(
      int agentId,
      std::map<std::string, sensor::ObservationSpace>& spaces);

  nav::PathFinder::ptr getPathFinder();
  void setPathFinder(nav::PathFinder::ptr pf);

  /**
   * @brief Enable or disable frustum culling (enabled by default)
   * @param val true = enable, false = disable
   */
  void setFrustumCullingEnabled(bool val) { frustumCulling_ = val; }

  /**
   * @brief Get status, whether frustum culling is enabled or not
   * @return true if enabled, otherwise false
   */
  bool isFrustumCullingEnabled() { return frustumCulling_; }

  /**
   * @brief Get a copy of an existing @ref gfx::LightSetup by its key.
   *
   * @param key The string key of the @ref gfx::LightSetup.
   */
  gfx::LightSetup getLightSetup(
      const std::string& key = assets::ResourceManager::DEFAULT_LIGHTING_KEY);

  /**
   * @brief Register a @ref gfx::LightSetup with a key name.
   *
   * If this name already exists, the @ref gfx::LightSetup is updated and all
   * @ref esp::gfx::Drawable s using this setup are updated.
   *
   * @param lightSetup The @ref gfx::LightSetup this key will now reference.
   * @param key Key to identify this @ref gfx::LightSetup.
   */
  void setLightSetup(
      gfx::LightSetup lightSetup,
      const std::string& key = assets::ResourceManager::DEFAULT_LIGHTING_KEY);

  /**
   * @brief Set the light setup of an object
   *
   * @param objectID The object ID and key identifying the object in @ref
   * esp::physics::PhysicsManager::existingObjects_.
   * @param lightSetupKey @ref gfx::LightSetup key
   * @param sceneID !! Not used currently !! Specifies which physical scene
   * of the object.
   */
  void setObjectLightSetup(int objectID,
                           const std::string& lightSetupKey,
                           int sceneID = 0);

  /**
   * @brief Getter for PRNG.
   *
   * Use this where-ever possible so that habitat won't be effected by
   * python random or numpy.random modules.
   */
  core::Random::ptr random() { return random_; }

  void overrideCollisionGroup(int objectID, int group);

  int createArticulatedP2PConstraint(int articulatedObjectId,
                                     int linkId,
                                     int objectId,
                                     float maxImpulse);

  int createArticulatedP2PConstraintWithPivots(int articulatedObjectId,
                                               int linkId,
                                               int objectId,
                                               const Magnum::Vector3& pivotA,
                                               const Magnum::Vector3& pivotB,
                                               float maxImpulse);

  int createArticulatedFixedConstraint(int articulatedObjectId,
                                       int linkId,
                                       int objectId,
                                       float maxImpulse);

  int createArticulatedFixedConstraintWithPivots(int articulatedObjectId,
                                                 int linkId,
                                                 int objectId,
                                                 const Magnum::Vector3& pivotA,
                                                 const Magnum::Vector3& pivotB,
                                                 float maxImpulse);

  void removeConstraint(int constraintId);

  int getPhysicsNumActiveContactPoints() {
    return physicsManager_->getNumActiveContactPoints();
  }
  int getPhysicsNumActiveOverlappingPairs() {
    return physicsManager_->getNumActiveOverlappingPairs();
  }
  std::string getPhysicsStepCollisionSummary() {
    return physicsManager_->getStepCollisionSummary();
  }

 protected:
  Simulator(){};

  //! sample a random valid AgentState in passed agentState
  void sampleRandomAgentState(agent::AgentState& agentState);

  bool isValidScene(int sceneID) const {
    return sceneID >= 0 && sceneID < sceneID_.size();
  }

  bool sceneHasPhysics(int sceneID) const {
    return isValidScene(sceneID) && physicsManager_ != nullptr;
  }

  gfx::WindowlessContext::uptr context_ = nullptr;
  std::shared_ptr<gfx::Renderer> renderer_ = nullptr;
  // CANNOT make the specification of resourceManager_ above the context_!
  // Because when deconstructing the resourceManager_, it needs
  // the GL::Context
  // If you switch the order, you will have the error:
  // GL::Context::current(): no current context from Magnum
  // during the deconstruction
  std::unique_ptr<assets::ResourceManager> resourceManager_ = nullptr;

  scene::SceneManager::uptr sceneManager_ = nullptr;
  int activeSceneID_ = ID_UNDEFINED;
  int activeSemanticSceneID_ = ID_UNDEFINED;
  std::vector<int> sceneID_;

  std::shared_ptr<scene::SemanticScene> semanticScene_ = nullptr;

  std::shared_ptr<physics::PhysicsManager> physicsManager_ = nullptr;

  core::Random::ptr random_;
  SimulatorConfiguration config_;

  std::vector<agent::Agent::ptr> agents_;
  nav::PathFinder::ptr pathfinder_;
  // state indicating frustum culling is enabled or not
  //
  // TODO:
  // Such state, frustumCulling_ has also been defined in frontend (py)
  // See: examples/settings.py, habitat_sim/simulator.py for more information
  // ideally, to avoid inconsistency at any time, and reduce maintenance cost
  // this state should be defined in just one place.e.g., only in the frontend
  // Currently, we need it defined here, because sensor., e.g., PinholeCamera
  // rquires it when drawing the observation
  bool frustumCulling_ = true;

  //! NavMesh visualization variables
  int navMeshVisPrimID_ = esp::ID_UNDEFINED;
  esp::scene::SceneNode* navMeshVisNode_ = nullptr;

  ESP_SMART_POINTERS(Simulator)
};

}  // namespace sim
}  // namespace esp

#endif  // ESP_SIM_SIMULATOR_H_
