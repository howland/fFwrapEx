from python_string cimport PyString_FromStringAndSize as PyBytes_FromStringAndSize
ctypedef str fw_bytes
cdef extern from "fwrap_ktp_header.h":
    enum:
        FW_ARR_DIM__ = 2
        FW_CHAR_SIZE__ = 1
        FW_INIT_ERR__ = -1
        FW_NO_ERR__ = 0
        fw_errstr_len = 63
    ctypedef int fwi_integer_t
    ctypedef char fw_character_t
    ctypedef long int fwi_npy_intp_t
    ctypedef double fwr_real_8_t
