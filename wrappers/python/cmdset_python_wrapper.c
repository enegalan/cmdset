#include <Python.h>
#include "cmdset.h"

static PyObject* py_cmdset_init(PyObject* self, PyObject* args) {
    cmdset_manager_t* manager = (cmdset_manager_t*)malloc(sizeof(cmdset_manager_t));
    if (!manager) {
        PyErr_SetString(PyExc_RuntimeError, "Memory allocation failed");
        return NULL;
    }
    int result = cmdset_init(manager);
    if (result != 0) {
        free(manager);
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize CmdSet");
        return NULL;
    }
    PyObject* manager_obj = PyLong_FromVoidPtr(manager);
    return manager_obj;
}

static PyObject* py_cmdset_add_preset(PyObject* self, PyObject* args) {
    PyObject* manager_obj;
    char* name;
    char* command;
    int encrypt = 0;
    if (!PyArg_ParseTuple(args, "Oss|i", &manager_obj, &name, &command, &encrypt)) {
        return NULL;
    }
    cmdset_manager_t* manager = (cmdset_manager_t*)PyLong_AsVoidPtr(manager_obj);
    if (!manager) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid manager object");
        return NULL;
    }
    int result = cmdset_add_preset(manager, name, command, encrypt);
    if (result != 0) {
        const char* error_msg = cmdset_get_error_message(result);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* py_cmdset_list_presets(PyObject* self, PyObject* args) {
    PyObject* manager_obj;
    if (!PyArg_ParseTuple(args, "O", &manager_obj)) return NULL;
    cmdset_manager_t* manager = (cmdset_manager_t*)PyLong_AsVoidPtr(manager_obj);
    if (!manager) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid manager object");
        return NULL;
    }
    int count = cmdset_get_preset_count(manager);
    PyObject* preset_list = PyList_New(0);
    for (int i = 0; i < count; i++) {
        cmdset_preset_t preset;
        if (cmdset_get_preset_by_index(manager, i, &preset) == 0) {
            PyObject* preset_dict = PyDict_New();
            PyDict_SetItemString(preset_dict, "name", PyUnicode_FromString(preset.name));
            PyDict_SetItemString(preset_dict, "command", PyUnicode_FromString(preset.command));
            PyDict_SetItemString(preset_dict, "encrypt", PyBool_FromLong(preset.encrypt));
            PyDict_SetItemString(preset_dict, "created_at", PyLong_FromLong(preset.created_at));
            PyDict_SetItemString(preset_dict, "last_used", PyLong_FromLong(preset.last_used));
            PyDict_SetItemString(preset_dict, "use_count", PyLong_FromLong(preset.use_count));
            PyList_Append(preset_list, preset_dict);
        }
    }
    return preset_list;
}

static PyObject* py_cmdset_execute_preset(PyObject* self, PyObject* args) {
    PyObject* manager_obj;
    char* name;
    char* additional_args = NULL;
    if (!PyArg_ParseTuple(args, "Os|s", &manager_obj, &name, &additional_args)) return NULL;
    cmdset_manager_t* manager = (cmdset_manager_t*)PyLong_AsVoidPtr(manager_obj);
    if (!manager) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid manager object");
        return NULL;
    }
    int result = cmdset_execute_preset(manager, name, additional_args);
    if (result < 0) {
        const char* error_msg = cmdset_get_error_message(result);
        PyErr_SetString(PyExc_RuntimeError, error_msg);
        return NULL;
    }
    return PyLong_FromLong(result);
}

static PyObject* py_cmdset_cleanup(PyObject* self, PyObject* args) {
    PyObject* manager_obj;
    if (!PyArg_ParseTuple(args, "O", &manager_obj)) return NULL;
    cmdset_manager_t* manager = (cmdset_manager_t*)PyLong_AsVoidPtr(manager_obj);
    if (manager) {
        cmdset_cleanup(manager);
        free(manager);
    }
    Py_RETURN_NONE;
}

static PyMethodDef cmdset_methods[] = {
    {"init", py_cmdset_init, METH_VARARGS, "Initialize CmdSet manager"},
    {"add_preset", py_cmdset_add_preset, METH_VARARGS, "Add a new preset"},
    {"list_presets", py_cmdset_list_presets, METH_VARARGS, "List all presets"},
    {"execute_preset", py_cmdset_execute_preset, METH_VARARGS, "Execute a preset"},
    {"cleanup", py_cmdset_cleanup, METH_VARARGS, "Cleanup CmdSet manager"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef cmdset_module = {
    PyModuleDef_HEAD_INIT,
    "cmdset",
    "CmdSet Python Extension",
    -1,
    cmdset_methods
};

PyMODINIT_FUNC PyInit_cmdset(void) {
    return PyModule_Create(&cmdset_module);
}
