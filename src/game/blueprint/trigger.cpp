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

#include "trigger.h"

#include <boost/algorithm/string.hpp>

using namespace std;

using namespace reone::resource;

namespace reone {

namespace game {

TriggerBlueprint::TriggerBlueprint(const string &resRef) : _resRef(resRef) {
}

void TriggerBlueprint::load(const GffStruct &utt) {
    _tag = utt.getString("Tag");
    boost::to_lower(_tag);

    _onEnter = utt.getString("ScriptOnEnter");
    _onExit = utt.getString("ScriptOnExit");
}

const string &TriggerBlueprint::tag() const {
    return _tag;
}

const string &TriggerBlueprint::onEnter() const {
    return _onEnter;
}

const string &TriggerBlueprint::onExit() const {
    return _onExit;
}

} // namespace resource

} // namespace reone
