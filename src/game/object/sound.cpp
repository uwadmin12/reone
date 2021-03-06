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

#include "sound.h"

#include "glm/glm.hpp"

#include "../../audio/files.h"
#include "../../audio/player.h"
#include "../../audio/soundhandle.h"
#include "../../resource/resources.h"

#include "../blueprint/blueprints.h"

using namespace std;

using namespace reone::audio;
using namespace reone::resource;
using namespace reone::scene;

namespace reone {

namespace game {

Sound::Sound(uint32_t id, SceneGraph *sceneGraph) : SpatialObject(id, ObjectType::Sound, sceneGraph) {
}

void Sound::load(const GffStruct &gffs) {
    string blueprintResRef(gffs.getString("TemplateResRef"));
    _blueprint = Blueprints::instance().getSound(blueprintResRef);

    _tag = _blueprint->tag();
    _active = _blueprint->active();

    shared_ptr<TwoDaTable> priorityGroups(Resources::instance().get2DA("prioritygroups"));
    _priority = priorityGroups->getInt(_blueprint->priority(), "priority");

    _position[0] = gffs.getFloat("XPosition");
    _position[1] = gffs.getFloat("YPosition");
    _position[2] = gffs.getFloat("ZPosition");

    updateTransform();
}

void Sound::update(float dt) {
    SpatialObject::update(dt);

    if (_sound) {
        if (_audible) {
            _sound->setPosition(getPosition());
        } else {
            _sound->stop();
            _sound.reset();
        }
    }
    if (!_active || !_audible) return;

    if (!_sound || _sound->isStopped()) {
        if (_timeout > 0.0f) {
            _timeout = glm::max(0.0f, _timeout - dt);
            return;
        }
        const vector<string> &sounds = _blueprint->sounds();
        int soundCount = static_cast<int>(sounds.size());
        if (sounds.empty()) {
            _active = false;
            return;
        } else if (++_soundIdx == soundCount) {
            if (_blueprint->looping()) {
                _soundIdx = 0;
            } else {
                _active = false;
                return;
            }
        }
        playSound(sounds[_soundIdx], soundCount == 1 && _blueprint->continuous());
        _timeout = _blueprint->interval() / 1000.0f;
    }
}

void Sound::playSound(const string &resRef, bool loop) {
    float gain = _blueprint->volume() / 127.0f;
    _sound = AudioPlayer::instance().play(resRef, AudioType::Sound, loop, gain, _blueprint->positional(), getPosition());
}

void Sound::play() {
    if (_sound) {
        _sound->stop();
    }

    _timeout = 0.0f;
    _active = true;
}

void Sound::stop() {
    if (_sound) {
        _sound->stop();
        _sound.reset();
    }

    _active = false;
}

shared_ptr<SoundBlueprint> Sound::blueprint() const {
    return _blueprint;
}

bool Sound::isActive() const {
    return _active;
}

glm::vec3 Sound::getPosition() const {
    glm::vec3 position(_transform[3]);
    position.z += _blueprint->elevation();
    return move(position);
}

int Sound::priority() const {
    return _priority;
}

void Sound::setAudible(bool audible) {
    _audible = audible;
}

} // namespace game

} // namespace reone
