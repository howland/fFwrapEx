"""
The one_arg module was generated with Fwrap v0.2.0_dev.

Below is a listing of functions and data types.
For usage information see the function docstrings.

Functions
---------
one_arg(...)

Data Types
----------
fw_character
fwi_integer
fwi_npy_intp
fwr_real_8

"""
np.import_array()
include 'fwrap_ktp.pxi'
cdef extern from "string.h":
    void *memcpy(void *dest, void *src, size_t n)
cpdef api object one_arg(object a):
    """
    one_arg(a) -> (fw_ret_arg, a)

    Parameters
    ----------
    a : fwr_real_8, 2D array, dimension(:, :), intent inout

    Returns
    -------
    fw_ret_arg : fwi_integer, intent out
    a : fwr_real_8, 2D array, dimension(:, :), intent inout

    """
    cdef fwi_integer_t fw_ret_arg
    cdef np.ndarray[fwr_real_8_t, ndim=2, mode='fortran'] a_
    cdef fwi_integer_t fw_iserr__
    cdef fw_character_t fw_errstr__[fw_errstr_len]
    a_ = np.PyArray_FROMANY(a, fwr_real_8_t_enum, 2, 2, np.NPY_F_CONTIGUOUS)
    one_arg_c(&fw_ret_arg, <fwi_npy_intp_t*>&a_.shape[0], <fwi_npy_intp_t*>&a_.shape[1], <fwr_real_8_t*>a_.data, &fw_iserr__, fw_errstr__)
    if fw_iserr__ != FW_NO_ERR__:
        raise RuntimeError("an error was encountered when calling the 'one_arg' wrapper.")
    return (fw_ret_arg, a_,)
