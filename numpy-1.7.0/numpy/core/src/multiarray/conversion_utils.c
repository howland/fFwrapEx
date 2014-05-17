#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#define NPY_NO_DEPRECATED_API NPY_API_VERSION
#define _MULTIARRAYMODULE
#include "numpy/arrayobject.h"
#include "numpy/arrayscalars.h"
#include "numpy/arrayobject.h"

#include "npy_config.h"
#include "npy_pycompat.h"

#include "common.h"
#include "arraytypes.h"

#include "conversion_utils.h"

/****************************************************************
* Useful function for conversion when used with PyArg_ParseTuple
****************************************************************/

/*NUMPY_API
 *
 * Useful to pass as converter function for O& processing in PyArgs_ParseTuple.
 *
 * This conversion function can be used with the "O&" argument for
 * PyArg_ParseTuple.  It will immediately return an object of array type
 * or will convert to a NPY_ARRAY_CARRAY any other object.
 *
 * If you use PyArray_Converter, you must DECREF the array when finished
 * as you get a new reference to it.
 */
NPY_NO_EXPORT int
PyArray_Converter(PyObject *object, PyObject **address)
{
    if (PyArray_Check(object)) {
        *address = object;
        Py_INCREF(object);
        return NPY_SUCCEED;
    }
    else {
        *address = PyArray_FromAny(object, NULL, 0, 0,
                                NPY_ARRAY_CARRAY, NULL);
        if (*address == NULL) {
            return NPY_FAIL;
        }
        return NPY_SUCCEED;
    }
}

/*NUMPY_API
 * Useful to pass as converter function for O& processing in
 * PyArgs_ParseTuple for output arrays
 */
NPY_NO_EXPORT int
PyArray_OutputConverter(PyObject *object, PyArrayObject **address)
{
    if (object == NULL || object == Py_None) {
        *address = NULL;
        return NPY_SUCCEED;
    }
    if (PyArray_Check(object)) {
        *address = (PyArrayObject *)object;
        return NPY_SUCCEED;
    }
    else {
        PyErr_SetString(PyExc_TypeError,
                        "output must be an array");
        *address = NULL;
        return NPY_FAIL;
    }
}

/*NUMPY_API
 * Get intp chunk from sequence
 *
 * This function takes a Python sequence object and allocates and
 * fills in an intp array with the converted values.
 *
 * Remember to free the pointer seq.ptr when done using
 * PyDimMem_FREE(seq.ptr)**
 */
NPY_NO_EXPORT int
PyArray_IntpConverter(PyObject *obj, PyArray_Dims *seq)
{
    int len;
    int nd;

    seq->ptr = NULL;
    seq->len = 0;
    if (obj == Py_None) {
        return NPY_SUCCEED;
    }
    len = PySequence_Size(obj);
    if (len == -1) {
        /* Check to see if it is a number */
        if (PyNumber_Check(obj)) {
            len = 1;
        }
    }
    if (len < 0) {
        PyErr_SetString(PyExc_TypeError,
                        "expected sequence object with len >= 0");
        return NPY_FAIL;
    }
    if (len > NPY_MAXDIMS) {
        PyErr_Format(PyExc_ValueError, "sequence too large; "
                     "must be smaller than %d", NPY_MAXDIMS);
        return NPY_FAIL;
    }
    if (len > 0) {
        seq->ptr = PyDimMem_NEW(len);
        if (seq->ptr == NULL) {
            PyErr_NoMemory();
            return NPY_FAIL;
        }
    }
    seq->len = len;
    nd = PyArray_IntpFromSequence(obj, (npy_intp *)seq->ptr, len);
    if (nd == -1 || nd != len) {
        PyDimMem_FREE(seq->ptr);
        seq->ptr = NULL;
        return NPY_FAIL;
    }
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Get buffer chunk from object
 *
 * this function takes a Python object which exposes the (single-segment)
 * buffer interface and returns a pointer to the data segment
 *
 * You should increment the reference count by one of buf->base
 * if you will hang on to a reference
 *
 * You only get a borrowed reference to the object. Do not free the
 * memory...
 */
NPY_NO_EXPORT int
PyArray_BufferConverter(PyObject *obj, PyArray_Chunk *buf)
{
    Py_ssize_t buflen;

    buf->ptr = NULL;
    buf->flags = NPY_ARRAY_BEHAVED;
    buf->base = NULL;
    if (obj == Py_None) {
        return NPY_SUCCEED;
    }
    if (PyObject_AsWriteBuffer(obj, &(buf->ptr), &buflen) < 0) {
        PyErr_Clear();
        buf->flags &= ~NPY_ARRAY_WRITEABLE;
        if (PyObject_AsReadBuffer(obj, (const void **)&(buf->ptr),
                                  &buflen) < 0) {
            return NPY_FAIL;
        }
    }
    buf->len = (npy_intp) buflen;

    /* Point to the base of the buffer object if present */
#if defined(NPY_PY3K)
    if (PyMemoryView_Check(obj)) {
        buf->base = PyMemoryView_GET_BASE(obj);
    }
#else
    if (PyBuffer_Check(obj)) {
        buf->base = ((PyArray_Chunk *)obj)->base;
    }
#endif
    if (buf->base == NULL) {
        buf->base = obj;
    }
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Get axis from an object (possibly None) -- a converter function,
 *
 * See also PyArray_ConvertMultiAxis, which also handles a tuple of axes.
 */
NPY_NO_EXPORT int
PyArray_AxisConverter(PyObject *obj, int *axis)
{
    if (obj == Py_None) {
        *axis = NPY_MAXDIMS;
    }
    else {
        *axis = (int) PyInt_AsLong(obj);
        if (PyErr_Occurred()) {
            return NPY_FAIL;
        }
    }
    return NPY_SUCCEED;
}

/*
 * Converts an axis parameter into an ndim-length C-array of
 * boolean flags, True for each axis specified.
 *
 * If obj is None or NULL, everything is set to True. If obj is a tuple,
 * each axis within the tuple is set to True. If obj is an integer,
 * just that axis is set to True.
 */
NPY_NO_EXPORT int
PyArray_ConvertMultiAxis(PyObject *axis_in, int ndim, npy_bool *out_axis_flags)
{
    /* None means all of the axes */
    if (axis_in == Py_None || axis_in == NULL) {
        memset(out_axis_flags, 1, ndim);
        return NPY_SUCCEED;
    }
    /* A tuple of which axes */
    else if (PyTuple_Check(axis_in)) {
        int i, naxes;

        memset(out_axis_flags, 0, ndim);

        naxes = PyTuple_Size(axis_in);
        if (naxes < 0) {
            return NPY_FAIL;
        }
        for (i = 0; i < naxes; ++i) {
            PyObject *tmp = PyTuple_GET_ITEM(axis_in, i);
            long axis = PyInt_AsLong(tmp);
            long axis_orig = axis;
            if (axis == -1 && PyErr_Occurred()) {
                return NPY_FAIL;
            }
            if (axis < 0) {
                axis += ndim;
            }
            if (axis < 0 || axis >= ndim) {
                PyErr_Format(PyExc_ValueError,
                        "'axis' entry %ld is out of bounds [-%d, %d)",
                        axis_orig, ndim, ndim);
                return NPY_FAIL;
            }
            if (out_axis_flags[axis]) {
                PyErr_SetString(PyExc_ValueError,
                        "duplicate value in 'axis'");
                return NPY_FAIL;
            }
            out_axis_flags[axis] = 1;
        }

        return NPY_SUCCEED;
    }
    /* Try to interpret axis as an integer */
    else {
        long axis, axis_orig;

        memset(out_axis_flags, 0, ndim);

        axis = PyInt_AsLong(axis_in);
        axis_orig = axis;
        /* TODO: PyNumber_Index would be good to use here */
        if (axis == -1 && PyErr_Occurred()) {
            return NPY_FAIL;
        }
        if (axis < 0) {
            axis += ndim;
        }
        /*
         * Special case letting axis={-1,0} slip through for scalars,
         * for backwards compatibility reasons.
         */
        if (ndim == 0 && (axis == 0 || axis == -1)) {
            return NPY_SUCCEED;
        }

        if (axis < 0 || axis >= ndim) {
            PyErr_Format(PyExc_ValueError,
                    "'axis' entry %ld is out of bounds [-%d, %d)",
                    axis_orig, ndim, ndim);
            return NPY_FAIL;
        }

        out_axis_flags[axis] = 1;

        return NPY_SUCCEED;
    }
}

/*NUMPY_API
 * Convert an object to true / false
 */
NPY_NO_EXPORT int
PyArray_BoolConverter(PyObject *object, npy_bool *val)
{
    if (PyObject_IsTrue(object)) {
        *val = NPY_TRUE;
    }
    else {
        *val = NPY_FALSE;
    }
    if (PyErr_Occurred()) {
        return NPY_FAIL;
    }
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert object to endian
 */
NPY_NO_EXPORT int
PyArray_ByteorderConverter(PyObject *obj, char *endian)
{
    char *str;
    PyObject *tmp = NULL;

    if (PyUnicode_Check(obj)) {
        obj = tmp = PyUnicode_AsASCIIString(obj);
    }

    *endian = NPY_SWAP;
    str = PyBytes_AsString(obj);
    if (!str) {
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    if (strlen(str) < 1) {
        PyErr_SetString(PyExc_ValueError,
                        "Byteorder string must be at least length 1");
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    *endian = str[0];
    if (str[0] != NPY_BIG && str[0] != NPY_LITTLE
        && str[0] != NPY_NATIVE && str[0] != NPY_IGNORE) {
        if (str[0] == 'b' || str[0] == 'B') {
            *endian = NPY_BIG;
        }
        else if (str[0] == 'l' || str[0] == 'L') {
            *endian = NPY_LITTLE;
        }
        else if (str[0] == 'n' || str[0] == 'N') {
            *endian = NPY_NATIVE;
        }
        else if (str[0] == 'i' || str[0] == 'I') {
            *endian = NPY_IGNORE;
        }
        else if (str[0] == 's' || str[0] == 'S') {
            *endian = NPY_SWAP;
        }
        else {
            PyErr_Format(PyExc_ValueError,
                         "%s is an unrecognized byteorder",
                         str);
            Py_XDECREF(tmp);
            return NPY_FAIL;
        }
    }
    Py_XDECREF(tmp);
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert object to sort kind
 */
NPY_NO_EXPORT int
PyArray_SortkindConverter(PyObject *obj, NPY_SORTKIND *sortkind)
{
    char *str;
    PyObject *tmp = NULL;

    if (PyUnicode_Check(obj)) {
        obj = tmp = PyUnicode_AsASCIIString(obj);
    }

    *sortkind = NPY_QUICKSORT;
    str = PyBytes_AsString(obj);
    if (!str) {
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    if (strlen(str) < 1) {
        PyErr_SetString(PyExc_ValueError,
                        "Sort kind string must be at least length 1");
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    if (str[0] == 'q' || str[0] == 'Q') {
        *sortkind = NPY_QUICKSORT;
    }
    else if (str[0] == 'h' || str[0] == 'H') {
        *sortkind = NPY_HEAPSORT;
    }
    else if (str[0] == 'm' || str[0] == 'M') {
        *sortkind = NPY_MERGESORT;
    }
    else {
        PyErr_Format(PyExc_ValueError,
                     "%s is an unrecognized kind of sort",
                     str);
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    Py_XDECREF(tmp);
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert object to searchsorted side
 */
NPY_NO_EXPORT int
PyArray_SearchsideConverter(PyObject *obj, void *addr)
{
    NPY_SEARCHSIDE *side = (NPY_SEARCHSIDE *)addr;
    char *str;
    PyObject *tmp = NULL;

    if (PyUnicode_Check(obj)) {
        obj = tmp = PyUnicode_AsASCIIString(obj);
    }

    str = PyBytes_AsString(obj);
    if (!str || strlen(str) < 1) {
        PyErr_SetString(PyExc_ValueError,
                        "expected nonempty string for keyword 'side'");
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }

    if (str[0] == 'l' || str[0] == 'L') {
        *side = NPY_SEARCHLEFT;
    }
    else if (str[0] == 'r' || str[0] == 'R') {
        *side = NPY_SEARCHRIGHT;
    }
    else {
        PyErr_Format(PyExc_ValueError,
                     "'%s' is an invalid value for keyword 'side'", str);
        Py_XDECREF(tmp);
        return NPY_FAIL;
    }
    Py_XDECREF(tmp);
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert an object to FORTRAN / C / ANY / KEEP
 */
NPY_NO_EXPORT int
PyArray_OrderConverter(PyObject *object, NPY_ORDER *val)
{
    char *str;
    /* Leave the desired default from the caller for NULL/Py_None */
    if (object == NULL || object == Py_None) {
        return NPY_SUCCEED;
    }
    else if (PyUnicode_Check(object)) {
        PyObject *tmp;
        int ret;
        tmp = PyUnicode_AsASCIIString(object);
        ret = PyArray_OrderConverter(tmp, val);
        Py_DECREF(tmp);
        return ret;
    }
    else if (!PyBytes_Check(object) || PyBytes_GET_SIZE(object) < 1) {
        if (PyObject_IsTrue(object)) {
            *val = NPY_FORTRANORDER;
        }
        else {
            *val = NPY_CORDER;
        }
        if (PyErr_Occurred()) {
            return NPY_FAIL;
        }
        return NPY_SUCCEED;
    }
    else {
        str = PyBytes_AS_STRING(object);
        if (str[0] == 'C' || str[0] == 'c') {
            *val = NPY_CORDER;
        }
        else if (str[0] == 'F' || str[0] == 'f') {
            *val = NPY_FORTRANORDER;
        }
        else if (str[0] == 'A' || str[0] == 'a') {
            *val = NPY_ANYORDER;
        }
        else if (str[0] == 'K' || str[0] == 'k') {
            *val = NPY_KEEPORDER;
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                            "order not understood");
            return NPY_FAIL;
        }
    }
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert an object to NPY_RAISE / NPY_CLIP / NPY_WRAP
 */
NPY_NO_EXPORT int
PyArray_ClipmodeConverter(PyObject *object, NPY_CLIPMODE *val)
{
    if (object == NULL || object == Py_None) {
        *val = NPY_RAISE;
    }
    else if (PyBytes_Check(object)) {
        char *str;
        str = PyBytes_AS_STRING(object);
        if (str[0] == 'C' || str[0] == 'c') {
            *val = NPY_CLIP;
        }
        else if (str[0] == 'W' || str[0] == 'w') {
            *val = NPY_WRAP;
        }
        else if (str[0] == 'R' || str[0] == 'r') {
            *val = NPY_RAISE;
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                            "clipmode not understood");
            return NPY_FAIL;
        }
    }
    else if (PyUnicode_Check(object)) {
        PyObject *tmp;
        int ret;
        tmp = PyUnicode_AsASCIIString(object);
        ret = PyArray_ClipmodeConverter(tmp, val);
        Py_DECREF(tmp);
        return ret;
    }
    else {
        int number = PyInt_AsLong(object);
        if (number == -1 && PyErr_Occurred()) {
            goto fail;
        }
        if (number <= (int) NPY_RAISE
                && number >= (int) NPY_CLIP) {
            *val = (NPY_CLIPMODE) number;
        }
        else {
            goto fail;
        }
    }
    return NPY_SUCCEED;

 fail:
    PyErr_SetString(PyExc_TypeError,
                    "clipmode not understood");
    return NPY_FAIL;
}

/*NUMPY_API
 * Convert an object to an array of n NPY_CLIPMODE values.
 * This is intended to be used in functions where a different mode
 * could be applied to each axis, like in ravel_multi_index.
 */
NPY_NO_EXPORT int
PyArray_ConvertClipmodeSequence(PyObject *object, NPY_CLIPMODE *modes, int n)
{
    int i;
    /* Get the clip mode(s) */
    if (object && (PyTuple_Check(object) || PyList_Check(object))) {
        if (PySequence_Size(object) != n) {
            PyErr_Format(PyExc_ValueError,
                    "list of clipmodes has wrong length (%d instead of %d)",
                    (int)PySequence_Size(object), n);
            return NPY_FAIL;
        }

        for (i = 0; i < n; ++i) {
            PyObject *item = PySequence_GetItem(object, i);
            if(item == NULL) {
                return NPY_FAIL;
            }

            if(PyArray_ClipmodeConverter(item, &modes[i]) != NPY_SUCCEED) {
                Py_DECREF(item);
                return NPY_FAIL;
            }

            Py_DECREF(item);
        }
    }
    else if (PyArray_ClipmodeConverter(object, &modes[0]) == NPY_SUCCEED) {
        for (i = 1; i < n; ++i) {
            modes[i] = modes[0];
        }
    }
    else {
        return NPY_FAIL;
    }
    return NPY_SUCCEED;
}

/*NUMPY_API
 * Convert any Python object, *obj*, to an NPY_CASTING enum.
 */
NPY_NO_EXPORT int
PyArray_CastingConverter(PyObject *obj, NPY_CASTING *casting)
{
    char *str = NULL;
    Py_ssize_t length = 0;

    if (PyUnicode_Check(obj)) {
        PyObject *str_obj;
        int ret;
        str_obj = PyUnicode_AsASCIIString(obj);
        if (str_obj == NULL) {
            return 0;
        }
        ret = PyArray_CastingConverter(str_obj, casting);
        Py_DECREF(str_obj);
        return ret;
    }

    if (PyBytes_AsStringAndSize(obj, &str, &length) == -1) {
        return 0;
    }

    if (length >= 2) switch (str[2]) {
        case 0:
            if (strcmp(str, "no") == 0) {
                *casting = NPY_NO_CASTING;
                return 1;
            }
            break;
        case 'u':
            if (strcmp(str, "equiv") == 0) {
                *casting = NPY_EQUIV_CASTING;
                return 1;
            }
            break;
        case 'f':
            if (strcmp(str, "safe") == 0) {
                *casting = NPY_SAFE_CASTING;
                return 1;
            }
            break;
        case 'm':
            if (strcmp(str, "same_kind") == 0) {
                *casting = NPY_SAME_KIND_CASTING;
                return 1;
            }
            break;
        case 's':
            if (strcmp(str, "unsafe") == 0) {
                *casting = NPY_UNSAFE_CASTING;
                return 1;
            }
            break;
    }

    PyErr_SetString(PyExc_ValueError,
            "casting must be one of 'no', 'equiv', 'safe', "
            "'same_kind', or 'unsafe'");
    return 0;
}

/*****************************
* Other conversion functions
*****************************/

/*NUMPY_API*/
NPY_NO_EXPORT int
PyArray_PyIntAsInt(PyObject *o)
{
    long long_value = -1;
    PyObject *obj;
    static char *msg = "an integer is required";
    PyArrayObject *arr;
    PyArray_Descr *descr;
    int ret;


    if (!o) {
        PyErr_SetString(PyExc_TypeError, msg);
        return -1;
    }
    if (PyInt_Check(o)) {
        long_value = (long) PyInt_AS_LONG(o);
        goto finish;
    } else if (PyLong_Check(o)) {
        long_value = (long) PyLong_AsLong(o);
        goto finish;
    }

    descr = &INT_Descr;
    arr = NULL;
    if (PyArray_Check(o)) {
        if (PyArray_SIZE((PyArrayObject *)o)!=1 ||
                                !PyArray_ISINTEGER((PyArrayObject *)o)) {
            PyErr_SetString(PyExc_TypeError, msg);
            return -1;
        }
        Py_INCREF(descr);
        arr = (PyArrayObject *)PyArray_CastToType((PyArrayObject *)o,
                                                    descr, 0);
    }
    if (PyArray_IsScalar(o, Integer)) {
        Py_INCREF(descr);
        arr = (PyArrayObject *)PyArray_FromScalar(o, descr);
    }
    if (arr != NULL) {
        ret = *((int *)PyArray_DATA(arr));
        Py_DECREF(arr);
        return ret;
    }
#if (PY_VERSION_HEX >= 0x02050000)
    if (PyIndex_Check(o)) {
        PyObject* value = PyNumber_Index(o);
        long_value = (npy_longlong) PyInt_AsSsize_t(value);
        goto finish;
    }
#endif
    if (Py_TYPE(o)->tp_as_number != NULL &&
        Py_TYPE(o)->tp_as_number->nb_int != NULL) {
        obj = Py_TYPE(o)->tp_as_number->nb_int(o);
        if (obj == NULL) {
            return -1;
        }
        long_value = (long) PyLong_AsLong(obj);
        Py_DECREF(obj);
    }
#if !defined(NPY_PY3K)
    else if (Py_TYPE(o)->tp_as_number != NULL &&
             Py_TYPE(o)->tp_as_number->nb_long != NULL) {
        obj = Py_TYPE(o)->tp_as_number->nb_long(o);
        if (obj == NULL) {
            return -1;
        }
        long_value = (long) PyLong_AsLong(obj);
        Py_DECREF(obj);
    }
#endif
    else {
        PyErr_SetString(PyExc_NotImplementedError,"");
    }

 finish:
    if error_converting(long_value) {
            PyErr_SetString(PyExc_TypeError, msg);
            return -1;
        }

#if (NPY_SIZEOF_LONG > NPY_SIZEOF_INT)
    if ((long_value < INT_MIN) || (long_value > INT_MAX)) {
        PyErr_SetString(PyExc_ValueError, "integer won't fit into a C int");
        return -1;
    }
#endif
    return (int) long_value;
}

/*NUMPY_API*/
NPY_NO_EXPORT npy_intp
PyArray_PyIntAsIntp(PyObject *o)
{
    npy_longlong long_value = -1;
    PyObject *obj;
    static char *msg = "an integer is required";
    PyArrayObject *arr;
    PyArray_Descr *descr;
    npy_intp ret;

    if (!o) {
        PyErr_SetString(PyExc_TypeError, msg);
        return -1;
    }
    if (PyInt_Check(o)) {
        long_value = (npy_longlong) PyInt_AS_LONG(o);
        goto finish;
    } else if (PyLong_Check(o)) {
        long_value = (npy_longlong) PyLong_AsLongLong(o);
        goto finish;
    }

#if NPY_SIZEOF_INTP == NPY_SIZEOF_LONG
    descr = &LONG_Descr;
#elif NPY_SIZEOF_INTP == NPY_SIZEOF_INT
    descr = &INT_Descr;
#else
    descr = &LONGLONG_Descr;
#endif
    arr = NULL;

    if (PyArray_Check(o)) {
        if (PyArray_SIZE((PyArrayObject *)o)!=1 ||
                                !PyArray_ISINTEGER((PyArrayObject *)o)) {
            PyErr_SetString(PyExc_TypeError, msg);
            return -1;
        }
        Py_INCREF(descr);
        arr = (PyArrayObject *)PyArray_CastToType((PyArrayObject *)o,
                                                                descr, 0);
    }
    else if (PyArray_IsScalar(o, Integer)) {
        Py_INCREF(descr);
        arr = (PyArrayObject *)PyArray_FromScalar(o, descr);
    }
    if (arr != NULL) {
        ret = *((npy_intp *)PyArray_DATA(arr));
        Py_DECREF(arr);
        return ret;
    }

#if (PY_VERSION_HEX >= 0x02050000)
    if (PyIndex_Check(o)) {
        PyObject* value = PyNumber_Index(o);
        if (value == NULL) {
            return -1;
        }
        long_value = (npy_longlong) PyInt_AsSsize_t(value);
        goto finish;
    }
#endif
#if !defined(NPY_PY3K)
    if (Py_TYPE(o)->tp_as_number != NULL &&                 \
        Py_TYPE(o)->tp_as_number->nb_long != NULL) {
        obj = Py_TYPE(o)->tp_as_number->nb_long(o);
        if (obj != NULL) {
            long_value = (npy_longlong) PyLong_AsLongLong(obj);
            Py_DECREF(obj);
        }
    }
    else
#endif
    if (Py_TYPE(o)->tp_as_number != NULL &&                 \
             Py_TYPE(o)->tp_as_number->nb_int != NULL) {
        obj = Py_TYPE(o)->tp_as_number->nb_int(o);
        if (obj != NULL) {
            long_value = (npy_longlong) PyLong_AsLongLong(obj);
            Py_DECREF(obj);
        }
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError,"");
    }

 finish:
    if error_converting(long_value) {
            PyErr_SetString(PyExc_TypeError, msg);
            return -1;
        }

#if (NPY_SIZEOF_LONGLONG > NPY_SIZEOF_INTP)
    if ((long_value < NPY_MIN_INTP) || (long_value > NPY_MAX_INTP)) {
        PyErr_SetString(PyExc_ValueError,
                        "integer won't fit into a C intp");
        return -1;
    }
#endif
    return (npy_intp) long_value;
}

/*NUMPY_API
 * PyArray_IntpFromSequence
 * Returns the number of dimensions or -1 if an error occurred.
 * vals must be large enough to hold maxvals
 */
NPY_NO_EXPORT int
PyArray_IntpFromSequence(PyObject *seq, npy_intp *vals, int maxvals)
{
    int nd, i;
    PyObject *op, *err;

    /*
     * Check to see if sequence is a single integer first.
     * or, can be made into one
     */
    if ((nd=PySequence_Length(seq)) == -1) {
        if (PyErr_Occurred()) PyErr_Clear();
#if NPY_SIZEOF_LONG >= NPY_SIZEOF_INTP && !defined(NPY_PY3K)
        if (!(op = PyNumber_Int(seq))) {
            return -1;
        }
#else
        if (!(op = PyNumber_Long(seq))) {
            return -1;
        }
#endif
        nd = 1;
#if NPY_SIZEOF_LONG >= NPY_SIZEOF_INTP
        vals[0] = (npy_intp ) PyInt_AsLong(op);
#else
        vals[0] = (npy_intp ) PyLong_AsLongLong(op);
#endif
        Py_DECREF(op);

        /*
         * Check wether there was an error - if the error was an overflow, raise
         * a ValueError instead to be more helpful
         */
        if(vals[0] == -1) {
            err = PyErr_Occurred();
            if (err  &&
                PyErr_GivenExceptionMatches(err, PyExc_OverflowError)) {
                PyErr_SetString(PyExc_ValueError,
                        "Maximum allowed dimension exceeded");
            }
            if(err != NULL) {
                return -1;
            }
        }
    }
    else {
        for (i = 0; i < PyArray_MIN(nd,maxvals); i++) {
            op = PySequence_GetItem(seq, i);
            if (op == NULL) {
                return -1;
            }
#if NPY_SIZEOF_LONG >= NPY_SIZEOF_INTP
            vals[i]=(npy_intp )PyInt_AsLong(op);
#else
            vals[i]=(npy_intp )PyLong_AsLongLong(op);
#endif
            Py_DECREF(op);

            /*
             * Check wether there was an error - if the error was an overflow,
             * raise a ValueError instead to be more helpful
             */
            if(vals[0] == -1) {
                err = PyErr_Occurred();
                if (err  &&
                    PyErr_GivenExceptionMatches(err, PyExc_OverflowError)) {
                    PyErr_SetString(PyExc_ValueError,
                            "Maximum allowed dimension exceeded");
                }
                if(err != NULL) {
                    return -1;
                }
            }
        }
    }
    return nd;
}

/**
 * WARNING: This flag is a bad idea, but was the only way to both
 *   1) Support unpickling legacy pickles with object types.
 *   2) Deprecate (and later disable) usage of O4 and O8
 *
 * The key problem is that the pickled representation unpickles by
 * directly calling the dtype constructor, which has no way of knowing
 * that it is in an unpickle context instead of a normal context without
 * evil global state like we create here.
 */
NPY_NO_EXPORT int evil_global_disable_warn_O4O8_flag = 0;

/*NUMPY_API
 * Typestr converter
 */
NPY_NO_EXPORT int
PyArray_TypestrConvert(int itemsize, int gentype)
{
    int newtype = NPY_NOTYPE;
    PyArray_Descr *temp;
    const char *msg = "Specified size is invalid for this data type.\n"
        "Size will be ignored in NumPy 1.7 but may throw an exception in future versions.";

    switch (gentype) {
        case NPY_GENBOOLLTR:
            if (itemsize == 1) {
                newtype = NPY_BOOL;
            }
            break;

        case NPY_SIGNEDLTR:
            switch(itemsize) {
                case 1:
                    newtype = NPY_INT8;
                    break;
                case 2:
                    newtype = NPY_INT16;
                    break;
                case 4:
                    newtype = NPY_INT32;
                    break;
                case 8:
                    newtype = NPY_INT64;
                    break;
#ifdef NPY_INT128
                case 16:
                    newtype = NPY_INT128;
                    break;
#endif
            }
            break;

        case NPY_UNSIGNEDLTR:
            switch(itemsize) {
                case 1:
                    newtype = NPY_UINT8;
                    break;
                case 2:
                    newtype = NPY_UINT16;
                    break;
                case 4:
                    newtype = NPY_UINT32;
                    break;
                case 8:
                    newtype = NPY_UINT64;
                    break;
#ifdef NPY_INT128
                case 16:
                    newtype = NPY_UINT128;
                    break;
#endif
            }
            break;

        case NPY_FLOATINGLTR:
            switch(itemsize) {
                case 2:
                    newtype = NPY_FLOAT16;
                    break;
                case 4:
                    newtype = NPY_FLOAT32;
                    break;
                case 8:
                    newtype = NPY_FLOAT64;
                    break;
#ifdef NPY_FLOAT80
                case 10:
                    newtype = NPY_FLOAT80;
                    break;
#endif
#ifdef NPY_FLOAT96
                case 12:
                    newtype = NPY_FLOAT96;
                    break;
#endif
#ifdef NPY_FLOAT128
                case 16:
                    newtype = NPY_FLOAT128;
                    break;
#endif
            }
            break;

        case NPY_COMPLEXLTR:
            switch(itemsize) {
                case 8:
                    newtype = NPY_COMPLEX64;
                    break;
                case 16:
                    newtype = NPY_COMPLEX128;
                    break;
#ifdef NPY_FLOAT80
                case 20:
                    newtype = NPY_COMPLEX160;
                    break;
#endif
#ifdef NPY_FLOAT96
                case 24:
                    newtype = NPY_COMPLEX192;
                    break;
#endif
#ifdef NPY_FLOAT128
                case 32:
                    newtype = NPY_COMPLEX256;
                    break;
#endif
            }
            break;

        case NPY_OBJECTLTR:
            /*
             * For 'O4' and 'O8', let it pass, but raise a
             * deprecation warning. For all other cases, raise
             * an exception by leaving newtype unset.
             */
            if (itemsize == 4 || itemsize == 8) {
                int ret = 0;
                if (evil_global_disable_warn_O4O8_flag) {
                    ret = DEPRECATE("DType strings 'O4' and 'O8' are "
                            "deprecated because they are platform "
                            "specific. Use 'O' instead");
                }

                if (ret == 0) {
                    newtype = NPY_OBJECT;
                }
            }
            break;

        case NPY_STRINGLTR:
        case NPY_STRINGLTR2:
            newtype = NPY_STRING;
            break;

        case NPY_UNICODELTR:
            newtype = NPY_UNICODE;
            break;

        case NPY_VOIDLTR:
            newtype = NPY_VOID;
            break;

        case NPY_DATETIMELTR:
            if (itemsize == 8) {
                newtype = NPY_DATETIME;
            }
            break;

        case NPY_TIMEDELTALTR:
            if (itemsize == 8) {
                newtype = NPY_TIMEDELTA;
            }
            break;
    }
   
    /*
     * Raise deprecate warning if new type hasn't been
     * set yet and size char is invalid.
     * This should eventually be changed to an error in
     * future NumPy versions.
     */
    if (newtype == NPY_NOTYPE) {
        temp = PyArray_DescrFromType(gentype);
        if (temp != NULL) {
            if (temp->elsize != itemsize) {
                if (DEPRECATE(msg) < 0) {
                    Py_DECREF(temp);
                    return -1;
                }

                newtype = gentype;
            }
            else {
                newtype = gentype;
            }

            Py_DECREF(temp);
        }
    }

    return newtype;
}

/* Lifted from numarray */
/* TODO: not documented */
/*NUMPY_API
  PyArray_IntTupleFromIntp
*/
NPY_NO_EXPORT PyObject *
PyArray_IntTupleFromIntp(int len, npy_intp *vals)
{
    int i;
    PyObject *intTuple = PyTuple_New(len);

    if (!intTuple) {
        goto fail;
    }
    for (i = 0; i < len; i++) {
#if NPY_SIZEOF_INTP <= NPY_SIZEOF_LONG
        PyObject *o = PyInt_FromLong((long) vals[i]);
#else
        PyObject *o = PyLong_FromLongLong((npy_longlong) vals[i]);
#endif
        if (!o) {
            Py_DECREF(intTuple);
            intTuple = NULL;
            goto fail;
        }
        PyTuple_SET_ITEM(intTuple, i, o);
    }

 fail:
    return intTuple;
}
