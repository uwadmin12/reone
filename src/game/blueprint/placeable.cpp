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

#include "placeable.h"

#include <boost/algorithm/string.hpp>

#include "../../resource/resources.h"

using namespace std;

using namespace reone::resource;

namespace reone {

namespace game {

PlaceableBlueprint::PlaceableBlueprint(const string &resRef) : _resRef(resRef) {
}

void PlaceableBlueprint::load(const GffStruct &utp) {
    _tag = utp.getString("Tag");
    boost::to_lower(_tag);

    int locNameStrRef = utp.getInt("LocName", -1);
    if (locNameStrRef != -1) {
        _localizedName = Resources::instance().getString(locNameStrRef);
    }

    _conversation = utp.getString("Conversation");
    _appearance = utp.getInt("Appearance");
    _hasInventory = utp.getInt("HasInventory") != 0;
    _usable = utp.getInt("Useable") != 0;

    loadItems(utp);
    loadScripts(utp);
}

void PlaceableBlueprint::loadItems(const GffStruct &utp) {
    const GffField *itemList = utp.find("ItemList");
    if (itemList) {
        for (auto &item : itemList->children()) {
            string resRef(item.getString("InventoryRes"));
            boost::to_lower(resRef);

            _items.push_back(move(resRef));
        }
    }
}

void PlaceableBlueprint::loadScripts(const GffStruct &utp) {
    _scripts.insert(make_pair(ScriptType::OnInvDisturbed, utp.getString("OnInvDisturbed")));
}

bool PlaceableBlueprint::getScript(ScriptType type, string &resRef) const {
    auto script = _scripts.find(type);
    if (script == _scripts.end() || script->second.empty()) return false;

    resRef = script->second;

    return true;
}

const string &PlaceableBlueprint::tag() const {
    return _tag;
}

const string &PlaceableBlueprint::localizedName() const {
    return _localizedName;
}

const string &PlaceableBlueprint::conversation() const {
    return _conversation;
}

int PlaceableBlueprint::appearance() const {
    return _appearance;
}

bool PlaceableBlueprint::hasInventory() const {
    return _hasInventory;
}

bool PlaceableBlueprint::isUsable() const {
    return _usable;
}

const vector<string> &PlaceableBlueprint::items() const {
    return _items;
}

} // namespace game

} // namespace reone
