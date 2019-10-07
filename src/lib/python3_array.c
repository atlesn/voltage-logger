/*

Read Route Record

Copyright (C) 2019 Atle Solbakken atle@goliathdns.no

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Python.h>
#include <structmember.h>

#include "linked_list.h"
#include "python3_module_common.h"
#include "python3_array.h"

struct rrr_python3_array_value_data;
struct rrr_python3_array_data;

/********************************************************************************
 * ARRAY VALUE
 ********************************************************************************/

struct rrr_python3_array_value_data {
	PyObject_HEAD
	PyObject *tag;
	PyObject *list;
	uint8_t type_orig;
};

void rrr_python3_array_value_set_tag (struct rrr_python3_array_value_data *node, PyObject *tag) {
	Py_XDECREF(node->tag);
	if (tag != NULL) {
		Py_INCREF(tag);
	}
	node->tag = tag;
}

static void rrr_python3_array_value_f_dealloc (PyObject *self) {
	struct rrr_python3_array_value_data *value = (struct rrr_python3_array_value_data *) self;
	Py_XDECREF(value->tag);
	Py_XDECREF(value->list);
	PyObject_Del(self);
}

static PyObject *rrr_python3_array_value_f_new (PyTypeObject *type, PyObject *args, PyObject *kwds) {
	PyObject *self = PyType_GenericNew(type, args, kwds);
	if (self == NULL) {
		VL_MSG_ERR("Could not create new value in rrr_python3_array_value_f_new\n");
		goto out_err;
	}

	struct rrr_python3_array_value_data *value = (struct rrr_python3_array_value_data *) self;

	value->list = PyList_New(0);
	if (value->list == NULL) {
		VL_MSG_ERR("Could not allocate memory for list in rrr_python3_array_value_f_new\n");
		goto out_err;
	}

	value->tag = PyUnicode_FromString("");
	if (value->list == NULL) {
		VL_MSG_ERR("Could not allocate memory for tag in rrr_python3_array_value_f_new\n");
		goto out_err;
	}

	value->type_orig = 0;

	return self;

	out_err:
		Py_XDECREF(self);
		return NULL;
}

static PyObject *rrr_python3_array_value_f_iter (PyObject *self) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	return PyObject_GetIter(data->list);
}

static PyObject *rrr_python3_array_value_f_remove (PyObject *self, PyObject *arg) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	PyObject *new_list = NULL;

	if (!PyLong_Check(arg)) {
		VL_MSG_ERR("Argument to rrr_array_value.remove() was not an integer\n");
		goto out_err;
	}

	long idx = PyLong_AsLong(arg);
	if (idx < 0) {
		VL_MSG_ERR("Argument to rrr_array_value.remove() was negative\n");
		goto out_err;
	}

	long size = PyList_GET_SIZE(data->list);

	if (idx >= size) {
		VL_MSG_ERR("Element out of range in rrr_array_value.remove()\n");
		goto out_err;
	}

	new_list = PyList_New(size - 1);
	if (new_list == NULL) {
		VL_MSG_ERR("Could not create new list in python3_array_value_f_remove\n");
		goto out_err;
	}

	int wpos = 0;
	for (int i = 0; i < size; i++) {
		if (i != idx) {
			PyObject *item = PyList_GET_ITEM(data->list, i);
			Py_INCREF(item);
			PyList_SET_ITEM(new_list, wpos++, item);
		}
	}

	Py_DECREF(data->list);
	data->list = new_list;

	Py_RETURN_TRUE;

	out_err:
		Py_XDECREF(new_list);
		Py_RETURN_FALSE;
}

static PyObject *rrr_python3_array_value_f_get_tag (PyObject *self, PyObject *arg) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	(void)(arg);

	Py_INCREF(data->tag);
	return data->tag;
}

static PyObject *rrr_python3_array_value_f_set_tag (PyObject *self, PyObject *arg) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	if (!PyUnicode_Check(arg)) {
		VL_MSG_ERR("Argument to rrr_array_value.set_tag() was not a string\n");
		Py_RETURN_FALSE;
	}
	Py_XDECREF(data->tag);
	data->tag = arg;
	Py_INCREF(arg);
	Py_RETURN_TRUE;
}

static PyObject *rrr_python3_array_value_f_get (PyObject *self, PyObject *arg_idx) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;

	if (!PyLong_Check(arg_idx)) {
		VL_MSG_ERR("First argument to rrr_array_value.set() was not an integer\n");
		Py_RETURN_FALSE;
	}

	long idx = PyLong_AsLong(arg_idx);
	if (idx < 0) {
		VL_MSG_ERR("Index given to rrr_array_value.set() was negative\n");
		Py_RETURN_FALSE;
	}

	long size = PyList_GET_SIZE(data->list);

	if (idx > size - 1) {
		VL_MSG_ERR("Index out of range in rrr_array_value.get()\n");
		Py_RETURN_NONE;
	}

	PyObject *result = PyList_GET_ITEM(data->list, idx);
	Py_INCREF(result);
	return result;
}

static PyObject *rrr_python3_array_value_f_set (PyObject *self, PyObject *args[], ssize_t count) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;

	if (count != 2) {
		VL_MSG_ERR("Arguments given to rrr_array_value.set() must be 2, one index and one value\n");
		Py_RETURN_FALSE;
	}

	PyObject *arg_idx = args[0];
	PyObject *value = args[1];

	if (!PyLong_Check(arg_idx)) {
		VL_MSG_ERR("First argument to rrr_array_value.set() was not an integer\n");
		Py_RETURN_FALSE;
	}

	long idx = PyLong_AsLong(arg_idx);
	if (idx < 0) {
		VL_MSG_ERR("Index given to rrr_array_value.set() was negative\n");
		Py_RETURN_FALSE;
	}

	long size = PyList_GET_SIZE(data->list);

	if (idx > size) {
		VL_MSG_ERR("Index out of range in rrr_array_value.set()\n");
		Py_RETURN_FALSE;
	}
	else if (idx < size) {
		Py_DECREF(PyList_GET_ITEM(data->list, idx));
		Py_INCREF(value);
		PyList_SET_ITEM(data->list, idx, value);
	}
	else {
		if (PyList_Append(data->list, value) != 0) {
			VL_MSG_ERR("Could not append value in rrr_python3_array_value_f_set\n");
			Py_RETURN_FALSE;
		}
	}

	Py_RETURN_TRUE;
}

static PyObject *rrr_python3_array_value_f_append (PyObject *self, PyObject *arg) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	if (PyList_Append(data->list, arg) != 0) {
		VL_MSG_ERR("Could not append item to value list in rrr_python3_array_value_f_append\n");
		Py_RETURN_FALSE;
	}
	Py_RETURN_TRUE;
}

static PyObject *rrr_python3_array_value_f_count (PyObject *self) {
	struct rrr_python3_array_value_data *data = (struct rrr_python3_array_value_data *) self;
	long count = PyList_GET_SIZE(data->list);
	PyObject *result = PyLong_FromLong(count);
	if (result == NULL) {
		VL_MSG_ERR("Could not create long in rrr_python3_array_value_f_count\n");
		Py_RETURN_NONE;
	}
	return result;
}

static PyMethodDef array_value_methods[] = {
		{
				.ml_name	= "remove",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_remove,
				.ml_flags	= METH_O,
				.ml_doc		= "Remove a data parameter at position x"
		},
		{
				.ml_name	= "get",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_get,
				.ml_flags	= METH_O,
				.ml_doc		= "Get a value of parameter with index x"
		},
		{
				.ml_name	= "set",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_set,
				.ml_flags	= METH_FASTCALL,
				.ml_doc		= "Set a value of parameter with index x to value y"
		},
		{
				.ml_name	= "get_tag",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_get_tag,
				.ml_flags	= METH_NOARGS,
				.ml_doc		= "Get tag of value"
		},
		{
				.ml_name	= "set_tag",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_set_tag,
				.ml_flags	= METH_O,
				.ml_doc		= "Set tag of value to x"
		},
		{
				.ml_name	= "append",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_append,
				.ml_flags	= METH_O,
				.ml_doc		= "Append a value x"
		},
		{
				.ml_name	= "count",
				.ml_meth	= (PyCFunction) rrr_python3_array_value_f_count,
				.ml_flags	= METH_NOARGS,
				.ml_doc		= "Get the number of items in the array"
		},
		{ NULL, NULL, 0, NULL }
};

PyTypeObject rrr_python3_array_value_type = {
		.ob_base		= PyVarObject_HEAD_INIT(NULL, 0) // Comma is inside macro
	    .tp_name		= RRR_PYTHON3_MODULE_NAME	"." RRR_PYTHON3_ARRAY_TYPE_NAME "_value",
	    .tp_basicsize	= sizeof(struct rrr_python3_array_value_data),
		.tp_itemsize	= 0,
	    .tp_dealloc		= (destructor) rrr_python3_array_value_f_dealloc,
	    .tp_print		= NULL,
	    .tp_getattr		= NULL,
	    .tp_setattr		= NULL,
	    .tp_as_async	= NULL,
	    .tp_repr		= NULL,
	    .tp_as_number	= NULL,
	    .tp_as_sequence	= NULL,
	    .tp_as_mapping	= NULL,
	    .tp_hash		= NULL,
	    .tp_call		= NULL,
	    .tp_str			= NULL,
	    .tp_getattro	= NULL,
	    .tp_setattro	= NULL,
	    .tp_as_buffer	= NULL,
	    .tp_flags		= Py_TPFLAGS_DEFAULT,
	    .tp_doc			= "ReadRouteRecord member type for VL Message Array structure",
	    .tp_traverse	= NULL,
	    .tp_clear		= NULL,
	    .tp_richcompare	= NULL,
	    .tp_weaklistoffset = 0,
	    .tp_iter		= rrr_python3_array_value_f_iter,
	    .tp_iternext	= NULL,
	    .tp_methods		= array_value_methods,
	    .tp_members		= NULL,
	    .tp_getset		= NULL,
	    .tp_base		= NULL,
	    .tp_dict		= NULL,
	    .tp_descr_get	= NULL,
	    .tp_descr_set	= NULL,
	    .tp_dictoffset	= 0,
	    .tp_init		= NULL,
	    .tp_alloc		= NULL,
	    .tp_new			= rrr_python3_array_value_f_new,
	    .tp_free		= NULL,
	    .tp_is_gc		= NULL,
	    .tp_bases		= NULL,
	    .tp_mro			= NULL,
	    .tp_cache		= NULL,
	    .tp_subclasses	= NULL,
	    .tp_weaklist	= NULL,
	    .tp_del			= NULL,
	    .tp_version_tag	= 0,
	    .tp_finalize	= NULL
};
/*
static int __rrr_python3_array_value_check (PyObject *self) {
	return(self->ob_type == &rrr_python3_array_value_type);
}
*/
/********************************************************************************
 * ARRAY
 ********************************************************************************/

struct rrr_python3_array_data {
	PyObject_HEAD
	PyObject *list;
};

static void rrr_python3_array_f_dealloc (PyObject *self) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;
	Py_XDECREF(data->list);
	PyObject_Del(self);
}

int rrr_python3_array_iterate (
		PyObject *self,
		int (*callback)(PyObject *tag, PyObject *list, uint8_t type_orig, void *arg),
		void *callback_arg
) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;

	int ret = 0;
	ssize_t max = PyList_GET_SIZE(data->list);
	for (ssize_t i = 0; i < max; i++) {
		PyObject *node = PyList_GET_ITEM(data->list, i);
		struct rrr_python3_array_value_data *value = (struct rrr_python3_array_value_data *) node;
		if ((ret = callback(value->tag, value->list, value->type_orig, callback_arg)) != 0) {
			VL_MSG_ERR("Error from callback in rrr_python3_array_iterate\n");
			goto out;
		}
		node = NULL;
	}

	out:
	return ret;
}

static struct rrr_python3_array_value_data *__rrr_python3_array_get_node_by_index (
		struct rrr_python3_array_data *data,
		int index
) {
	return (struct rrr_python3_array_value_data *) PyList_GetItem(data->list, index);
}

static struct rrr_python3_array_value_data *__rrr_python3_array_get_node_by_tag (
		struct rrr_python3_array_data *data,
		PyObject *tag
) {
	ssize_t max = PyList_GET_SIZE(data->list);
	for (ssize_t i = 0; i < max; i++) {
		PyObject *node = PyList_GET_ITEM(data->list, i);
		struct rrr_python3_array_value_data *value = (struct rrr_python3_array_value_data *) node;
		if (value->tag != NULL && PyUnicode_Compare(value->tag, tag) == 0) {
			return value;
		}
	}

	return NULL;
}

static PyObject *rrr_python3_array_f_new (PyTypeObject *type, PyObject *args, PyObject *kwds) {
	PyObject *self = PyType_GenericNew(type, args, kwds);
	if (self == NULL) {
		return NULL;
	}

	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;

	data->list = PyList_New(0);
	if (data->list == NULL) {
		VL_MSG_ERR("Could not create list in rrr_python3_array_f_new\n");
		goto out_err;
	}

	return self;

	out_err:
		Py_XDECREF(self);
		return NULL;
}

/*
static struct rrr_python3_array_value_data *rrr_python3_array_get_or_append_new (
		struct rrr_python3_array_data *data,
		int index
) {
	struct rrr_python3_array_value_data *node = NULL;

	ssize_t node_count = PyList_GET_SIZE(data->list);

	// Append new node if index is just after the current element count
	if (index == node_count) {
		node = PyObject_New(struct rrr_python3_array_value_data, &rrr_python3_array_value_type);
		if (node == NULL) {
			return NULL;
		}
		PyList_Append(data->list, (PyObject*) node);
	}
	else if (index > node_count) {
		VL_MSG_ERR("Index was too big in rrr_python3_array_set_value, max index is the current last index + 1, otherwise a hole would have been produced\n");
		return NULL;
	}
	else {
		node = __rrr_python3_array_get_node_by_index(data, index);
	}

	if (node == NULL) {
		VL_BUG("node was NULL in rrr_python3_array_set_value\n");
	}

	return node;
}

static int __rrr_python3_array_get_index_from_args (long *index_final, PyObject *args[], PyObject *count) {
	*index_final = 0;

	long argc = PyLong_AsLong(count);
	if (argc < 1) {
		VL_MSG_ERR("Missing index argument\n");
		return 1;
	}

	if (!PyLong_Check(args[0])) {
		VL_MSG_ERR("Non-numeric type specified as index\n");
		return 1;
	}

	long index = PyLong_AsLong(args[0]);
	if (index < 0) {
		VL_MSG_ERR("Negative index value provided\n");
		return 1;
	}

	*index_final = index;

	return 0;
}
*/

// Append a single value or value being an iterable (converted to PyList)
int rrr_python3_array_append (
		PyObject *self,
		PyObject *tag,
		PyObject *value,
		uint8_t type_orig
) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;

	PyObject *iterator = NULL;
	PyObject *item = NULL;

	struct rrr_python3_array_value_data *result = (struct rrr_python3_array_value_data *) rrr_python3_array_value_f_new(&rrr_python3_array_value_type, NULL, NULL);
	if (result == NULL) {
		VL_MSG_ERR("Could not allocate array value in rrr_python3_array_append\n");
		goto out_err;
	}

	result->type_orig = type_orig;
	rrr_python3_array_value_set_tag(result, tag);

	// Make sure we do not accidently iterate strings etc. and put one byte at a time into the tuple
	if (!PyUnicode_Check(value) && !PyByteArray_Check(value) && (iterator = PyObject_GetIter(value)) != NULL) {
		while ((item = PyIter_Next(iterator)) != NULL) {
			// append will incref
			if (PyList_Append(result->list, item) != 0) {
				VL_MSG_ERR("Could not append item to list in rrr_python3_array_append\n");
				goto out_err;
			}
			Py_DECREF(item);
			item = NULL;
		}
		Py_XDECREF(iterator);
		iterator = NULL;
	}
	else {
		PyErr_Clear();
		if (PyList_Append(result->list, value) != 0) {
			VL_MSG_ERR("Could not append item to list in rrr_python3_array_append\n");
			goto out_err;
		}
	}

	if (PyList_Append(data->list, (PyObject *) result) != 0) {
		VL_MSG_ERR("Could not append new value to list in rrr_python3_array_append\n");
		goto out_err;
	}

	result = NULL;

	return 0;

	out_err:
		Py_XDECREF(iterator);
		Py_XDECREF(value);
		Py_XDECREF(result);
		return 1;
}

static PyObject *rrr_python3_array_f_append (PyObject *self, PyObject *args[], ssize_t count) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;

	if (count != 2) {
		VL_MSG_ERR("Wrong number of arguments to rrr_array.append(), only tag and value may be given\n");
		Py_RETURN_FALSE;
	}

	PyObject *tag = args[0];
	PyObject *value = args[1];

	if (rrr_python3_array_append(self, tag, value, 0) != 0) {
		VL_MSG_ERR("Could not append tag and value in rrr_array.append()\n");
		Py_RETURN_NONE;
	}

	PyObject *result = PyList_GET_ITEM(data->list, PyList_GET_SIZE(data->list) - 1);
	Py_INCREF(result);
	return result;
}

static PyObject *rrr_python3_array_f_get_by_tag_or_index (PyObject *self, PyObject *tag) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;
	PyObject *value = NULL;

	if (PyUnicode_Check(tag)) {
		value = (PyObject *) __rrr_python3_array_get_node_by_tag(data, tag);
		if (value == NULL) {
			VL_MSG_ERR("Tag '%s' not found in rrr_array.get()\n", PyUnicode_AsUTF8(tag));
			Py_RETURN_NONE;
		}
	}
	else if (PyLong_Check(tag)) {
		long index = PyLong_AsLong(tag);
		if (index < 0) {
			VL_MSG_ERR("Negative index given to rrr_array.get()\n");
			Py_RETURN_NONE;
		}
		value = (PyObject *) __rrr_python3_array_get_node_by_index(data, index);
		if (value == NULL) {
			VL_MSG_ERR("Could not get node with index %li in rrr_array.get()\n", index);
			Py_RETURN_NONE;
		}
	}
	else {
		VL_MSG_ERR("Tag argument to rrr_array.get() was not a string or integer\n");
		Py_RETURN_NONE;
	}

	Py_INCREF(value);
	return value;
}

static PyObject *rrr_python3_array_f_iter (PyObject *self) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;
	return PyObject_GetIter(data->list);
}

static PyObject *rrr_python3_array_f_count (PyObject *self) {
	struct rrr_python3_array_data *data = (struct rrr_python3_array_data *) self;

	PyObject *result = PyLong_FromLong(rrr_python3_array_count(data));
	if (result == NULL) {
		VL_MSG_ERR("Could not create Long-object in rrr_python3_array_count\n");
		PyErr_Print();
		Py_RETURN_NONE;
	}

	return result;
}

static PyMethodDef array_methods[] = {
		{
				.ml_name	= "get",
				.ml_meth	= (PyCFunction) rrr_python3_array_f_get_by_tag_or_index,
				.ml_flags	= METH_O,
				.ml_doc		= "Get a value of parameter with tag or index x"
		},
		{
				.ml_name	= "append",
				.ml_meth	= (PyCFunction) rrr_python3_array_f_append,
				.ml_flags	= METH_FASTCALL,
				.ml_doc		= "Append a tag x and value y"
		},
		{
				.ml_name	= "count",
				.ml_meth	= (PyCFunction) rrr_python3_array_f_count,
				.ml_flags	= METH_NOARGS,
				.ml_doc		= "Get the number of items in the array"
		},
		{ NULL, NULL, 0, NULL }
};

static PyMemberDef array_members[] = {
		{ NULL, 0, 0, 0, NULL}
};

PyTypeObject rrr_python3_array_type = {
		.ob_base		= PyVarObject_HEAD_INIT(NULL, 0) // Comma is inside macro
	    .tp_name		= RRR_PYTHON3_MODULE_NAME	"." RRR_PYTHON3_ARRAY_TYPE_NAME,
	    .tp_basicsize	= sizeof(struct rrr_python3_array_data),
		.tp_itemsize	= 0,
	    .tp_dealloc		= (destructor) rrr_python3_array_f_dealloc,
	    .tp_print		= NULL,
	    .tp_getattr		= NULL,
	    .tp_setattr		= NULL,
	    .tp_as_async	= NULL,
	    .tp_repr		= NULL,
	    .tp_as_number	= NULL,
	    .tp_as_sequence	= NULL,
	    .tp_as_mapping	= NULL,
	    .tp_hash		= NULL,
	    .tp_call		= NULL,
	    .tp_str			= NULL,
	    .tp_getattro	= NULL,
	    .tp_setattro	= NULL,
	    .tp_as_buffer	= NULL,
	    .tp_flags		= Py_TPFLAGS_DEFAULT,
	    .tp_doc			= "ReadRouteRecord type for VL Message Array structure",
	    .tp_traverse	= NULL,
	    .tp_clear		= NULL,
	    .tp_richcompare	= NULL,
	    .tp_weaklistoffset = 0,
	    .tp_iter		= rrr_python3_array_f_iter,
	    .tp_iternext	= NULL,
	    .tp_methods		= array_methods,
	    .tp_members		= array_members,
	    .tp_getset		= NULL,
	    .tp_base		= NULL,
	    .tp_dict		= NULL,
	    .tp_descr_get	= NULL,
	    .tp_descr_set	= NULL,
	    .tp_dictoffset	= 0,
	    .tp_init		= NULL,
	    .tp_alloc		= NULL,
	    .tp_new			= rrr_python3_array_f_new,
	    .tp_free		= NULL,
	    .tp_is_gc		= NULL,
	    .tp_bases		= NULL,
	    .tp_mro			= NULL,
	    .tp_cache		= NULL,
	    .tp_subclasses	= NULL,
	    .tp_weaklist	= NULL,
	    .tp_del			= NULL,
	    .tp_version_tag	= 0,
	    .tp_finalize	= NULL
};

int rrr_python3_array_check (PyObject *object) {
	return (object->ob_type == &rrr_python3_array_type);
}

PyObject *rrr_python3_array_new (void) {
	return (PyObject *) rrr_python3_array_f_new(&rrr_python3_array_type, NULL, NULL);
}
