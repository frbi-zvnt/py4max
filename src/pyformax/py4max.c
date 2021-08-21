#include "ext.h"
#include "ext_buffer.h"
#include "ext_obex.h"
#include <Python.h>
#include <string.h>
#include <unistd.h>

#define DEBUG   1

char* NO_BUFFER_DATA        = "NO BUFFER DATA";
char* NO_MODULE_STRING      = "NO MODULE IMPORTED";
char* NO_PYTHON_STRING      = "PYTHON NOT INITIALIZED";
char* NO_FUNCTION_STRING    = "FUNCTION NOT FOUND";
char* CALL_FAILED_STRING    = "FUNCTION CALL FAILED";

#define NO_MODULE(a)    object_warn((t_object *)a, NO_MODULE_STRING, 0);
#define NO_PYTHON(a)    object_warn((t_object *)a, NO_PYTHON_STRING, 0);
#define NO_FUNC(a)      object_warn((t_object *)a, NO_FUNCTION_STRING, 0);
#define CALL_FAILED(a)  object_warn((t_object *)a, CALL_FAILED_STRING, 0);
#define WARNING(a,b,c)  object_warn((t_object *)a, b, c);


typedef struct _py4max {
    t_object        l_obj;
    t_buffer_ref*   buf_ref;
    long            n_chan;
    long            framecount;
    long            sr;
    void            *p_outlet;
    
    // python objects
    char*           pyPath;
    char*           scriptPath;
    PyObject*       pModule;
    PyObject*       pFuncName;
} t_py4max;


void py4max_set(t_py4max* x, t_symbol* s);
void* py4max_new(t_symbol* s);
void py4max_free(t_py4max* x);
void py4max_bang(t_py4max* x);
void py4max_dblclick(t_py4max* x);
void py4max_info(t_py4max* x);
t_max_err py4max_notify(t_py4max* x, t_symbol* s, t_symbol* msg, void* sender, void* data);


// python side
void py4max_pythonpath(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
void py4max_scriptpath(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
void py_init(t_py4max* x);
void py_finalize(void);
void py4max_import(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
void py4max_anything(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
void  run_script(t_py4max* x);
void py_set_script_path(t_py4max* x);


static t_class* py4max_class;


void ext_main(void* r)
{
    t_class* c = class_new("py4max", (method)py4max_new, (method)py4max_free, sizeof(t_py4max), NULL, A_SYM, 0);

    class_addmethod(c, (method)py4max_set, "set", A_SYM, 0);
    class_addmethod(c, (method)py4max_bang, "bang", A_CANT, 0);
    class_addmethod(c, (method)py4max_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)py4max_info, "info", 0);
    class_addmethod(c, (method)py4max_notify, "notify", A_CANT, 0);
    class_addmethod(c, (method)py4max_import, "import", A_GIMME, 0);
    class_addmethod(c, (method)py4max_anything, "anything", A_GIMME, 0);
    class_addmethod(c, (method)py4max_pythonpath, "pythonpath", A_GIMME, 0);
    class_addmethod(c, (method)py4max_scriptpath, "scriptpath", A_GIMME, 0);
    //class_dspinit(c);
    class_register(CLASS_BOX, c);
    py4max_class = c;
}


void py4max_bang(t_py4max* x)
{
    py_init(x);
}


void py4max_info(t_py4max* x)
{
    ;
}


void py4max_set(t_py4max* x, t_symbol* s)
{
    if (!x->buf_ref)
        x->buf_ref = buffer_ref_new((t_object*)x, s);
    else
        buffer_ref_set(x->buf_ref, s);
}


void py4max_dblclick(t_py4max* x)
{
    buffer_view(buffer_ref_getobject(x->buf_ref));
}


void* py4max_new(t_symbol* s)
{
    t_py4max* x = object_alloc(py4max_class);
    py4max_set(x, s);
    x->p_outlet = intout(x);
    return (x);
}


void py4max_free(t_py4max* x)
{
    if (x->pModule != NULL) Py_DECREF(x->pModule);
    py_finalize();
    object_free(x->buf_ref);
}


t_max_err py4max_notify(t_py4max* x, t_symbol* s, t_symbol* msg, void* sender, void* data)
{
    poststring(s->s_name);
    poststring(msg->s_name);
    return buffer_ref_notify(x->buf_ref, s, msg, sender, data);
}



/****************************************
* Python side
*****************************************/


void py4max_pythonpath(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    t_symbol *pa;
    char dst[1024];
    path_absolutepath(&pa, gensym("."), NULL, 0);
    path_nameconform(pa->s_name, dst, 0, 3);
    pa = gensym(dst);
    
    if (argc) pa = atom_getsym(&argv[0]);
    x->pyPath = pa->s_name;
    
#if DEBUG
    post_sym(x, gensym("PYTHON_PATH:"));
    post_sym(x, gensym(x->pyPath));
#endif
}


void py4max_scriptpath(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    t_symbol* pa = atom_getsym(&argv[0]);
    x->scriptPath = pa->s_name;
    py_set_script_path(x);
    
#if DEBUG
    post_sym(x, gensym("SCRIPT_SEARCH_PATH:"));
    post_sym(x, gensym(x->scriptPath));
#endif
}


void py_set_script_path(t_py4max* x) {
    if (x->scriptPath && Py_IsInitialized()) {
        PyObject *pList;
        //PyObject* pSys = PyUnicode_DecodeFSDefault("sys");
        //PyObject* pSysModule = PyImport_Import(pSys);
        PyObject* pSysModule = PyImport_ImportModule("sys");
        pList = PyObject_GetAttrString(pSysModule, (const char*)"path");
        
        if (PyList_Check(pList)){
            PyList_Append(pList, PyUnicode_DecodeFSDefault(x->scriptPath));
        }
        
#if DEBUG
        for (int i=0; i<PyList_Size(pList); i++) {
            PyObject *value = PyList_GetItem(pList, i);
            //poststring(Py_EncodeLocale(PyUnicode_AsWideCharString(value, NULL), NULL));
            post_sym(x, gensym(Py_EncodeLocale(PyUnicode_AsWideCharString(value, NULL), NULL)));
        }
#endif
        
        Py_DECREF(pList);
        Py_DECREF(pSysModule);
        //Py_DECREF(pSys);
    }
}


void py_init(t_py4max* x) {
    
    if (Py_IsInitialized()) Py_FinalizeEx();
    
    if (x->pyPath) {
        const wchar_t* home = Py_DecodeLocale(x->pyPath, NULL);
        Py_SetPythonHome(home);
    }
    
    const wchar_t* pgname = Py_DecodeLocale("/Users/neum/Documents/py4max/venvtest/bin/python3.9", NULL);
    Py_SetProgramName(pgname);

    Py_InitializeEx(0);
    py_set_script_path(x);
    
#if DEBUG
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("print('python version is:', sys.version)");
    PyRun_SimpleString("print(sys.prefix)");
    PyRun_SimpleString("print(sys.path)");
    wchar_t* pg_name = Py_GetProgramName();
    WARNING(x, "PROGRAM_NAME: %ls", pg_name);
    WARNING(x, "EXECPREFIX: %ls", Py_GetExecPrefix());
    WARNING(x, "PROGRAM FULLPATH: %ls", Py_GetProgramFullPath());
    WARNING(x, "PLATFORM: %s", Py_GetPlatform());
    WARNING(x, "PYTHONHOME: %ls", Py_GetPythonHome());
    WARNING(x, "GETPATH: %ls", Py_GetPath());
    WARNING(x, "GETVERSION: %s", Py_GetVersion());
    WARNING(x, "is initialized: %d", Py_IsInitialized());
#endif
}


void py_finalize() {
    Py_FinalizeEx();
}


void py4max_import(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    if (!Py_IsInitialized()) py_init(x);
    
    t_symbol* module = atom_getsym(&argv[0]);
    
#if DEBUG
    post("module name: %s", module->s_name);
#endif

    PyObject* pName = PyUnicode_DecodeFSDefault(module->s_name);
    Py_XDECREF(x->pModule);

    x->pModule = PyImport_Import(pName);
    //x->pModule = PyImport_ImportModule(module->s_name);
    Py_DECREF(pName);
    

    if (!x->pModule) {
        PyRun_SimpleString("print('MODULES: ')");
        PyRun_SimpleString("print(sys.modules)");
        NO_MODULE(x);
    }
}


void py4max_anything(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    if (x->pModule != NULL) {
        x->pFuncName = PyObject_GetAttrString(x->pModule, s->s_name);
         run_script(x);
    }
    else
        NO_MODULE(x);
}



PyObject* send_vector_to_py(t_py4max* x, t_buffer_obj* buf)
{
    PyObject *pList;
    PyObject *pArgs;
    PyObject *pValue;
    
    float *tab = buffer_locksamples(buf);
    
    // uses buffer values (if they exists) as function argument
    if (tab) {
        long framecount = buffer_getframecount(buf);
        
        // create empty list ([] in python)
        pList = PyList_New(0);
        pArgs = PyTuple_New(1);
        
        // append values to list
        while (framecount--) {
            PyList_Append(pList, PyFloat_FromDouble(*tab++));
        }
        
        PyTuple_SetItem(pArgs, 0, pList);
        pValue = PyObject_CallObject(x->pFuncName, pArgs);
        
        buffer_unlocksamples(buf);
        Py_DECREF(pArgs);
        return pValue;
    } else {
        poststring(NO_BUFFER_DATA);
        return NULL;
    }
}


void get_values_from_py(t_py4max* x, t_buffer_obj* buf, PyObject *pValue)
{
    if (pValue != NULL) {
        //post("value is NOT NULL");
        int size = (int)PyList_Size(pValue);
        
        float *tab = buffer_locksamples(buf);
        
        for (int i = 0; i < size; i++) {
            *tab++ = PyFloat_AsDouble(PyList_GetItem(pValue, i));
        }
        
        buffer_unlocksamples(buf);
        Py_DECREF(pValue);
    } else {
        Py_DECREF(x->pFuncName);
        Py_DECREF(x->pModule);
        PyErr_Print();
        CALL_FAILED(x);
    }
}


// RUN SCRIPT

void  run_script(t_py4max* x)
{
    PyObject *pValue;
    
    if (x->pModule != NULL) {
        
        if (x->pFuncName && PyCallable_Check(x->pFuncName)) {
            t_buffer_obj* buffer = buffer_ref_getobject(x->buf_ref);
            pValue = send_vector_to_py(x, buffer);
            get_values_from_py(x, buffer, pValue);
        } else {
#if DEBUG
            if (PyErr_Occurred())
                PyErr_Print();
#endif
            NO_FUNC(x);
        }
        
        Py_XDECREF(x->pFuncName);
    } else {
        PyErr_Print();
        NO_MODULE(x);
    }
}
