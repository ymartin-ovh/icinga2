/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2016 Icinga Development Team (https://www.icinga.org/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "base/array.hpp"
#include "base/function.hpp"
#include "base/functionwrapper.hpp"
#include "base/scriptframe.hpp"
#include "base/objectlock.hpp"
#include "base/exception.hpp"
#include <boost/foreach.hpp>

using namespace icinga;

static double ArrayLen(void)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	return self->GetLength();
}

static void ArraySet(int index, const Value& value)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	self->Set(index, value);
}

static Value ArrayGet(int index)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	return self->Get(index);
}

static void ArrayAdd(const Value& value)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	self->Add(value);
}

static void ArrayRemove(int index)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	self->Remove(index);
}

static bool ArrayContains(const Value& value)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	return self->Contains(value);
}

static void ArrayClear(void)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	self->Clear();
}

static bool ArraySortCmp(const Function::Ptr& cmp, const Value& a, const Value& b)
{
	std::vector<Value> args;
	args.push_back(a);
	args.push_back(b);
	return cmp->Invoke(args);
}

static Array::Ptr ArraySort(const std::vector<Value>& args)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	Array::Ptr arr = self->ShallowClone();

	if (args.empty()) {
		ObjectLock olock(arr);
		std::sort(arr->Begin(), arr->End());
	} else {
		Function::Ptr function = args[0];

		if (vframe->Sandboxed && !function->IsSideEffectFree())
			BOOST_THROW_EXCEPTION(ScriptError("Sort function must be side-effect free."));

		ObjectLock olock(arr);
		std::sort(arr->Begin(), arr->End(), boost::bind(ArraySortCmp, args[0], _1, _2));
	}

	return arr;
}

static Array::Ptr ArrayShallowClone(void)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	return self->ShallowClone();
}

static Value ArrayJoin(const Value& separator)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	Value result;
	bool first = true;

	ObjectLock olock(self);
	BOOST_FOREACH(const Value& item, self) {
		if (first) {
			first = false;
		} else {
			result = result + separator;
		}

		result = result + item;
	}

	return result;
}

static Array::Ptr ArrayReverse(void)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);
	return self->Reverse();
}

static Array::Ptr ArrayMap(const Function::Ptr& function)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	if (vframe->Sandboxed && !function->IsSideEffectFree())
		BOOST_THROW_EXCEPTION(ScriptError("Map function must be side-effect free."));

	Array::Ptr result = new Array();

	ObjectLock olock(self);
	BOOST_FOREACH(const Value& item, self) {
		ScriptFrame uframe;
		std::vector<Value> args;
		args.push_back(item);
		result->Add(function->Invoke(args));
	}

	return result;
}

static Value ArrayReduce(const Function::Ptr& function)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	if (vframe->Sandboxed && !function->IsSideEffectFree())
		BOOST_THROW_EXCEPTION(ScriptError("Reduce function must be side-effect free."));

	Value result;

	ObjectLock olock(self);
	BOOST_FOREACH(const Value& item, self) {
		ScriptFrame uframe;
		std::vector<Value> args;
		args.push_back(result);
		args.push_back(item);
		result = function->Invoke(args);
	}

	return result;
}

static Array::Ptr ArrayFilter(const Function::Ptr& function)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	if (vframe->Sandboxed && !function->IsSideEffectFree())
		BOOST_THROW_EXCEPTION(ScriptError("Filter function must be side-effect free."));

	Array::Ptr result = new Array();

	ObjectLock olock(self);
	BOOST_FOREACH(const Value& item, self) {
		ScriptFrame uframe;
		std::vector<Value> args;
		args.push_back(item);
		if (function->Invoke(args))
			result->Add(item);
	}

	return result;
}

static Array::Ptr ArrayUnique(void)
{
	ScriptFrame *vframe = ScriptFrame::GetCurrentFrame();
	Array::Ptr self = static_cast<Array::Ptr>(vframe->Self);

	std::set<Value> result;

	ObjectLock olock(self);
	BOOST_FOREACH(const Value& item, self) {
		result.insert(item);
	}

	return Array::FromSet(result);
}

Object::Ptr Array::GetPrototype(void)
{
	static Dictionary::Ptr prototype;

	if (!prototype) {
		prototype = new Dictionary();
		prototype->Set("len", new Function(WrapFunction(ArrayLen), true));
		prototype->Set("set", new Function(WrapFunction(ArraySet)));
		prototype->Set("get", new Function(WrapFunction(ArrayGet)));
		prototype->Set("add", new Function(WrapFunction(ArrayAdd)));
		prototype->Set("remove", new Function(WrapFunction(ArrayRemove)));
		prototype->Set("contains", new Function(WrapFunction(ArrayContains), true));
		prototype->Set("clear", new Function(WrapFunction(ArrayClear)));
		prototype->Set("sort", new Function(WrapFunction(ArraySort), true));
		prototype->Set("shallow_clone", new Function(WrapFunction(ArrayShallowClone), true));
		prototype->Set("join", new Function(WrapFunction(ArrayJoin), true));
		prototype->Set("reverse", new Function(WrapFunction(ArrayReverse), true));
		prototype->Set("map", new Function(WrapFunction(ArrayMap), true));
		prototype->Set("reduce", new Function(WrapFunction(ArrayReduce), true));
		prototype->Set("filter", new Function(WrapFunction(ArrayFilter), true));
		prototype->Set("unique", new Function(WrapFunction(ArrayUnique), true));
	}

	return prototype;
}

