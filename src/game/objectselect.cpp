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

#include "objectselect.h"

#include <stdexcept>

#include "glm/vec3.hpp"

#include "object/area.h"
#include "party.h"

using namespace std;

using namespace reone::render;
using namespace reone::scene;

namespace reone {

namespace game {

ObjectSelector::ObjectSelector(const Area *area, const Party *party) :
    _area(area), _party(party) {

    if (!area) {
        throw invalid_argument("area must not be null");
    }
    if (!party) {
        throw invalid_argument("party must not be null");
    }
}

void ObjectSelector::update() {
    if (_hilightedObjectId != -1) {
        shared_ptr<SpatialObject> object(_area->find(_hilightedObjectId));
        if (!object || !object->isSelectable()) {
            _hilightedObjectId = -1;
        }
    }
    if (_selectedObjectId != -1) {
        shared_ptr<SpatialObject> object(_area->find(_selectedObjectId));
        if (!object || !object->isSelectable()) {
            _selectedObjectId = -1;
        }
    }
}

void ObjectSelector::selectNext(bool reverse) {
    vector<uint32_t> selectables;
    getSelectableObjects(selectables);

    if (selectables.empty()) {
        _selectedObjectId = -1;
        return;
    }
    if (_selectedObjectId == -1) {
        _selectedObjectId = selectables.front();
        return;
    }
    if (reverse) {
        auto selected = std::find(selectables.rbegin(), selectables.rend(), _selectedObjectId);
        if (selected != selectables.rend()) {
            selected++;
        }
        _selectedObjectId = selected != selectables.rend() ? *selected : selectables.back();

    } else {
        auto selected = std::find(selectables.begin(), selectables.end(), _selectedObjectId);
        if (selected != selectables.end()) {
            selected++;
        }
        _selectedObjectId = selected != selectables.end() ? *selected : selectables.front();
    }
}

void ObjectSelector::getSelectableObjects(vector<uint32_t> &ids) const {
    vector<pair<uint32_t, float>> selectables;

    shared_ptr<SpatialObject> partyLeader(_party->leader());
    glm::vec3 origin(partyLeader->position());

    for (auto &object : _area->objects()) {
        if (!object->isSelectable() || object.get() == partyLeader.get()) continue;

        shared_ptr<ModelSceneNode> model(object->model());
        if (!model || !model->isVisible()) continue;

        float dist = object->distanceTo(origin);
        if (dist > kSelectionDistance) continue;

        selectables.push_back(make_pair(object->id(), dist));
    }

    sort(selectables.begin(), selectables.end(), [](const pair<uint32_t, float> &left, const pair<uint32_t, float> &right) {
        return left.second < right.second;
    });
    for (auto &selectable : selectables) {
        ids.push_back(selectable.first);
    }
}

void ObjectSelector::selectNearest() {
    _selectedObjectId = -1;
    selectNext();
}

void ObjectSelector::hilight(uint32_t objectId) {
    _hilightedObjectId = objectId;
}

void ObjectSelector::select(uint32_t objectId) {
    _selectedObjectId = objectId;
}

int ObjectSelector::hilightedObjectId() const {
    return _hilightedObjectId;
}

int ObjectSelector::selectedObjectId() const {
    return _selectedObjectId;
}

} // namespace game

} // namespace reone
