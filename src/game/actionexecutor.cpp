/*
 * Copyright (c) 2020 The reone project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "actionexecutor.h"

#include <stdexcept>

#include "SDL2/SDL_timer.h"

#include "../script/execution.h"
#include "../common/log.h"

#include "game.h"
#include "object/area.h"
#include "object/creature.h"
#include "object/door.h"
#include "object/placeable.h"
#include "script/util.h"

using namespace std;

using namespace reone::script;

namespace reone {

namespace game {

static const float kKeepPathDuration = 1000.0f;
static const float kDefaultMaxObjectDistance = 2.0f;
static const float kMaxConversationDistance = 4.0f;
static const float kDistanceWalk = 4.0f;

ActionExecutor::ActionExecutor(Game *game) : _game(game) {
    if (!game) {
        throw invalid_argument("game must not be null");
    }
}

void ActionExecutor::executeActions(Object &object, float dt) {
    ActionQueue &actionQueue = object.actionQueue();

    Action *action = actionQueue.currentAction();
    if (!action) return;

    ActionType type = action->type();
    switch (type) {
        case ActionType::MoveToPoint:
            executeMoveToPoint(static_cast<Creature &>(object), *dynamic_cast<MoveToPointAction *>(action), dt);
            break;
        case ActionType::MoveToObject:
            executeMoveToObject(static_cast<Creature &>(object), *dynamic_cast<MoveToObjectAction *>(action), dt);
            break;
        case ActionType::Follow:
            executeFollow(static_cast<Creature &>(object), *dynamic_cast<FollowAction *>(action), dt);
            break;
        case ActionType::DoCommand:
            executeDoCommand(object, *dynamic_cast<CommandAction *>(action), dt);
            break;
        case ActionType::StartConversation:
            executeStartConversation(object, *dynamic_cast<StartConversationAction *>(action), dt);
            break;
        case ActionType::AttackObject:
            executeAttack(static_cast<Creature &>(object), *dynamic_cast<AttackAction *>(action), dt);
            break;
        case ActionType::OpenDoor:
            executeOpenDoor(object, *dynamic_cast<ObjectAction *>(action), dt);
            break;
        case ActionType::CloseDoor:
            executeCloseDoor(object, *dynamic_cast<ObjectAction *>(action), dt);
            break;
        case ActionType::OpenContainer:
            executeOpenContainer(static_cast<Creature &>(object), *dynamic_cast<ObjectAction *>(action), dt);
            break;
        case ActionType::OpenLock:
            executeOpenLock(static_cast<Creature &>(object), *dynamic_cast<ObjectAction *>(action), dt);
            break;
        case ActionType::JumpToObject:
            executeJumpToObject(object, *dynamic_cast<ObjectAction *>(action), dt);
            break;
        case ActionType::JumpToLocation:
            executeJumpToLocation(object, *dynamic_cast<LocationAction *>(action), dt);
            break;
        default:
            warn("ActionExecutor: action not implemented: " + to_string(static_cast<int>(type)));
            action->complete();
            break;
    }
}

void ActionExecutor::executeMoveToPoint(Creature &actor, MoveToPointAction &action, float dt) {
    glm::vec3 dest(action.point());

    bool reached = navigateCreature(actor, dest, true, 1.0f, dt);
    if (reached) {
        action.complete();
    }
}

void ActionExecutor::executeMoveToObject(Creature &actor, MoveToObjectAction &action, float dt) {
    SpatialObject &object = *static_cast<SpatialObject *>(action.object());
    glm::vec3 dest(object.position());
    bool run = action.getRun();
    float distance = action.distance();

    bool reached = navigateCreature(actor, dest, run, distance, dt);
    if (reached) {
        action.complete();
    }
}

void ActionExecutor::executeFollow(Creature &actor, FollowAction &action, float dt) {
    SpatialObject &object = *static_cast<SpatialObject *>(action.object());
    glm::vec3 dest(object.position());
    float distance = actor.distanceTo(glm::vec2(dest));
    bool run = distance > kDistanceWalk;

    navigateCreature(actor, dest, run, action.distance(), dt);
}

void ActionExecutor::executeDoCommand(Object &actor, CommandAction &action, float dt) {
    ExecutionContext ctx(action.context());
    ctx.callerId = actor.id();

    ScriptExecution(ctx.savedState->program, move(ctx)).run();
    action.complete();
}

void ActionExecutor::executeStartConversation(Object &actor, StartConversationAction &action, float dt) {
    Creature *creatureActor = dynamic_cast<Creature *>(&actor);
    SpatialObject &object = static_cast<SpatialObject &>(*action.object());

    bool reached =
        !creatureActor ||
        action.isStartRangeIgnored() ||
        navigateCreature(*creatureActor, object.position(), true, kMaxConversationDistance, dt);

    if (reached) {
        bool isActorLeader = _game->party().leader()->id() == actor.id();
        _game->module()->area()->startDialog(isActorLeader ? object : static_cast<SpatialObject &>(actor), action.dialogResRef());
        action.complete();
    }
}

void ActionExecutor::executeAttack(Creature &actor, AttackAction &action, float dt) {
    shared_ptr<Creature> target(action.target());
    glm::vec3 dest(target->position());

    navigateCreature(actor, dest, true, action.range(), dt);
}

bool ActionExecutor::navigateCreature(Creature &creature, const glm::vec3 &dest, bool run, float distance, float dt) {
    if (creature.isMovementRestricted()) return false;

    const glm::vec3 &origin = creature.position();
    float distToDest = glm::distance2(origin, dest);

    if (distToDest <= distance * distance) {
        creature.setMovementType(Creature::MovementType::None);
        creature.clearPath();
        return true;
    }
    bool updatePath = true;
    shared_ptr<Creature::Path> path(creature.path());

    if (path) {
        uint32_t now = SDL_GetTicks();
        if (path->destination == dest || now - path->timeFound <= kKeepPathDuration) {
            advanceCreatureOnPath(creature, run, dt);
            updatePath = false;
        }
    }
    if (updatePath) {
        updateCreaturePath(creature, dest);
    }

    return false;
}

void ActionExecutor::advanceCreatureOnPath(Creature &creature, bool run, float dt) {
    const glm::vec3 &origin = creature.position();
    shared_ptr<Creature::Path> path(creature.path());
    size_t pointCount = path->points.size();
    glm::vec3 dest;
    float distToDest;

    if (path->pointIdx == pointCount) {
        dest = path->destination;
        distToDest = glm::distance2(origin, dest);

    } else {
        const glm::vec3 &nextPoint = path->points[path->pointIdx];
        float distToNextPoint = glm::distance2(origin, nextPoint);
        float distToPathDest = glm::distance2(origin, path->destination);

        if (distToPathDest < distToNextPoint) {
            dest = path->destination;
            distToDest = distToPathDest;
            path->pointIdx = static_cast<int>(pointCount);

        } else {
            dest = nextPoint;
            distToDest = distToNextPoint;
        }
    }

    if (distToDest <= 1.0f) {
        selectNextPathPoint(*path);

    } else if (_game->module()->area()->moveCreatureTowards(creature, dest, run, dt)) {
        creature.setMovementType(run ? Creature::MovementType::Run : Creature::MovementType::Walk);

    } else {
        creature.setMovementType(Creature::MovementType::None);
    }
}

void ActionExecutor::selectNextPathPoint(Creature::Path &path) {
    size_t pointCount = path.points.size();
    if (path.pointIdx < pointCount) {
        path.pointIdx++;
    }
}

void ActionExecutor::updateCreaturePath(Creature &creature, const glm::vec3 &dest) {
    const glm::vec3 &origin = creature.position();
    vector<glm::vec3> points(_game->module()->area()->pathfinder().findPath(origin, dest));
    uint32_t now = SDL_GetTicks();

    creature.setPath(dest, move(points), now);
}

void ActionExecutor::executeOpenDoor(Object &actor, ObjectAction &action, float dt) {
    Creature *creatureActor = dynamic_cast<Creature *>(&actor);
    Door &door = *static_cast<Door *>(action.object());

    bool reached = !creatureActor || navigateCreature(*creatureActor, door.position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        bool isObjectSelf = actor.id() == door.id();
        if (!isObjectSelf && door.isLocked()) {
            if (!door.blueprint().onFailToOpen().empty()) {
                runScript(door.blueprint().onFailToOpen(), door.id(), actor.id(), -1);
            }
        } else {
            door.open(&actor);
            if (!isObjectSelf && !door.blueprint().onOpen().empty()) {
                runScript(door.blueprint().onOpen(), door.id(), actor.id(), -1);
            }
        }
        action.complete();
    }
}

void ActionExecutor::executeCloseDoor(Object &actor, ObjectAction &action, float dt) {
    Creature *creatureActor = dynamic_cast<Creature *>(&actor);
    Door &door = *static_cast<Door *>(action.object());

    bool reached = !creatureActor || navigateCreature(*creatureActor, door.position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        door.close(&actor);
        action.complete();
    }
}

void ActionExecutor::executeOpenContainer(Creature &actor, ObjectAction &action, float dt) {
    Placeable &placeable = *static_cast<Placeable *>(action.object());
    bool reached = navigateCreature(actor, placeable.position(), true, kDefaultMaxObjectDistance, dt);
    if (reached) {
        _game->openContainer(&placeable);
        action.complete();
    }
}

void ActionExecutor::executeOpenLock(Creature &actor, ObjectAction &action, float dt) {
    Door *door = dynamic_cast<Door *>(action.object());
    if (door) {
        bool reached = navigateCreature(actor, door->position(), true, kDefaultMaxObjectDistance, dt);
        if (reached) {
            actor.face(*door);
            actor.playAnimation(Creature::Animation::UnlockDoor);
            door->setLocked(false);
            door->open(&actor);
            if (!door->blueprint().onOpen().empty()) {
                runScript(door->blueprint().onOpen(), door->id(), actor.id(), -1);
            }
            action.complete();
        }
    } else {
        warn("ActionExecutor: unsupported OpenLock object: " + to_string(action.object()->id()));
        action.complete();
    }
}

void ActionExecutor::executeJumpToObject(Object &actor, ObjectAction &action, float dt) {
    SpatialObject &spatialObject = *static_cast<SpatialObject *>(action.object());

    SpatialObject &spatialActor = static_cast<SpatialObject &>(actor);
    spatialActor.setPosition(spatialObject.position());
    spatialActor.setHeading(spatialObject.heading());

    action.complete();
}

void ActionExecutor::executeJumpToLocation(Object &actor, LocationAction &action, float dt) {
    SpatialObject &spatialActor = static_cast<SpatialObject &>(actor);
    spatialActor.setPosition(action.location()->position());
    spatialActor.setHeading(action.location()->facing());

    action.complete();
}

} // namespace game

} // namespace reone
