#include "ext.h"
//#include "z_dsp.h"
#include "ext_buffer.h"
//#include "ext_atomic.h"
#include "ext_obex.h"
#include <Python.h>

#define DEBUG   1

typedef struct _py4max {
    //t_pxobject      l_obj;
    t_object        l_obj;
    t_buffer_ref*   buf_ref;
    long            n_chan;
    long            framecount;
    long            sr;
    void            *p_outlet;
    t_symbol*       mModule;
} t_py4max;


void py4max_set(t_py4max* x, t_symbol* s);
void* py4max_new(t_symbol* s); //, long chan);
void py4max_free(t_py4max* x);
void py4max_bang(t_py4max* x);
void py4max_dblclick(t_py4max* x);
void py4max_info(t_py4max* x);
t_max_err py4max_notify(t_py4max* x, t_symbol* s, t_symbol* msg, void* sender, void* data);

// python side
void py_init(void);
void py_finalize(void);
void py4max_import(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
void py4max_anything(t_py4max* x, t_symbol* s, int argc, t_atom* argv);
float read_script(t_py4max* x, t_symbol* funcname, int argc, t_atom* argv);

static t_class* py4max_class;

void ext_main(void* r)
{
    t_class* c = class_new("py4max", (method)py4max_new, (method)py4max_free, sizeof(t_py4max), NULL, A_SYM, 0);

    //class_addmethod(c, (method)py4max_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)py4max_set, "set", A_SYM, 0);
    class_addmethod(c, (method)py4max_bang, "bang", A_CANT, 0);
    class_addmethod(c, (method)py4max_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)py4max_info, "info", 0);
    class_addmethod(c, (method)py4max_notify, "notify", A_CANT, 0);
    class_addmethod(c, (method)py4max_import, "import", A_GIMME, 0);
    class_addmethod(c, (method)py4max_anything, "anything", A_GIMME, 0);
    //class_dspinit(c);
    class_register(CLASS_BOX, c);
    py4max_class = c;
}


void py4max_bang(t_py4max* x)
{
    t_buffer_obj* buffer = buffer_ref_getobject(x->buf_ref);
    
    float *tab = buffer_locksamples(buffer);
    for (int i=0; i<100; i++)
        outlet_float(x->p_outlet, *tab++);
    buffer_unlocksamples(buffer);
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
    //dsp_setup((t_pxobject *)x, 1);
    py_init();
    py4max_set(x, s);
    x->p_outlet = intout(x);
    return (x);
}


void py4max_free(t_py4max* x)
{
    py_finalize();
    //dsp_free((t_pxobject*)x);
    object_free(x->buf_ref);
}


t_max_err py4max_notify(t_py4max* x, t_symbol* s, t_symbol* msg, void* sender, void* data)
{
    return buffer_ref_notify(x->buf_ref, s, msg, sender, data);
}



/****************************************
* Python side
*****************************************/


void py_init() {
    const wchar_t* home = Py_DecodeLocale("/Library/Frameworks/Python.framework/Versions/3.7", NULL);
    Py_SetPythonHome(home);
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/Users/neum/Documents/py4max\")");
    PyRun_SimpleString("print('python version is:', sys.version)");
    PyRun_SimpleString("print(sys.prefix)");
    PyRun_SimpleString("print(sys.path)");
    
    wchar_t* pg_name = Py_GetProgramName();
    
    post("PROGRAMNAME: %ls", pg_name);
    post("EXECPREFIX: %ls", Py_GetExecPrefix());
    post("PROGRAM FULLPATH: %ls", Py_GetProgramFullPath());
    post("PLATFORM: %s", Py_GetPlatform());
    post("PYTHONHOME: %ls", Py_GetPythonHome());
    post("GETPATH: %ls", Py_GetPath());
    post("GETVERSION: %s", Py_GetVersion());
    post("is initialized: %d", Py_IsInitialized());
}


void py_finalize() {
    Py_FinalizeEx();
}


void py4max_import(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    t_symbol* module = atom_getsym(&argv[0]);
    post("module name: %s", module->s_name);
    x->mModule = module;
}


void py4max_anything(t_py4max* x, t_symbol* s, int argc, t_atom* argv)
{
    post("funcname: %s", s->s_name);
    //postatom(argc, argv);
    float arg = atom_getfloat(&argv[0]);
    post("\n");
    
    read_script(x, s, argc, argv);
}


float read_script(t_py4max* x, t_symbol* funcname, int argc, t_atom* argv)
{
    PyObject *pName, *pModule, *pFunc, *pList;
    PyObject *pArgs, *pValue;
    
    //float result;
    
    if (x->mModule != NULL) {
        pName = PyUnicode_DecodeFSDefault(x->mModule->s_name);
        pModule = PyImport_Import(pName);
    } else {
        post("No module imported, import a module with import statement");
        return -1;
    }
    
    Py_DECREF(pName);
    
    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, funcname->s_name);
        /* pFunc is a new reference */
        
        if (pFunc && PyCallable_Check(pFunc)) {
            
            if (argc) {
                pList = PyList_New(argc);
                pArgs = PyTuple_New(1);
                
                for (int i = 0; i < argc; i++) {
                    post("%d", (t_int)atom_getfloat(&argv[i]));
                    PyList_SetItem(pList, i, PyLong_FromLong((t_int)atom_getfloat(&argv[i])));
                }
                
                post("lunghezza lista: %d", PyList_Size(pList));
                
                PyTuple_SetItem(pArgs, 0, pList);
                pValue = PyObject_CallObject(pFunc, pArgs);
                Py_DECREF(pArgs);
            } else {
                pArgs = NULL;
                pValue = PyObject_CallObject(pFunc, pArgs);
            }
            
            if (pValue != NULL) {
                
                int size = (int)PyList_Size(pValue);
                
                //outlet_float(x->x_obj.ob_outlet, read_script(x, funcname, arg));
                //outlet_list(x->x_obj.ob_outlet, &s_list, size);
                
                t_atom tastoma[size];
                
                //SETFLOAT(&x->result[i], x->l_list[i].a_w.w_float / x->r_list[i].a_w.w_float);
                
                for (int i = 0; i < size; i++) {
                    float item = PyFloat_AsDouble(PyList_GetItem(pValue, i));
                    atom_setfloat(&tastoma[i], item);
                }
                
                //t_atom *tastoma;
                
                Py_DECREF(pValue);
                outlet_list(x->p_outlet, gensym("list"), size, tastoma);
            } else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                //post(pValue);
                post("Call failed");
                return 1.0;
            }
        } else {
            if (PyErr_Occurred())
                PyErr_Print();
            post("Cannot find function");
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    } else {
        PyErr_Print();
        post("Failed to load module");
        return 1.0;
    }
    
    return 0;
}
