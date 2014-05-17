from fwrap_ktp cimport *

cdef extern from "fwproj_fc.h":
    void one_arg_c(fwi_integer_t *, fwi_npy_intp_t *, fwi_npy_intp_t *, fwr_real_8_t *, fwi_integer_t *, fw_character_t *)
