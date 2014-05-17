
module fwrap_ktp_mod
    use iso_c_binding
    implicit none
    integer, parameter :: FW_ARR_DIM__ = 2
    integer, parameter :: FW_CHAR_SIZE__ = 1
    integer, parameter :: FW_INIT_ERR__ = -1
    integer, parameter :: FW_NO_ERR__ = 0
    integer, parameter :: fw_errstr_len = 63
    integer, parameter :: fwi_integer_t = c_int
    integer, parameter :: fw_character_t = c_char
    integer, parameter :: fwi_npy_intp_t = c_long
    integer, parameter :: fwr_real_8_t = c_double
end module fwrap_ktp_mod
