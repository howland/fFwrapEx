subroutine one_arg_c(fw_ret_arg, a_d1, a_d2, a, fw_iserr__, fw_errstr__) bind&
&(c, name="one_arg_c")
    use fwrap_ktp_mod
    implicit none
    integer(kind=fwi_integer_t), intent(out) :: fw_ret_arg
    integer(kind=fwi_npy_intp_t), intent(in) :: a_d1
    integer(kind=fwi_npy_intp_t), intent(in) :: a_d2
    real(kind=fwr_real_8_t), dimension(a_d1, a_d2), intent(inout) :: a
    integer(kind=fwi_integer_t), intent(out) :: fw_iserr__
    character(kind=fw_character_t, len=1), dimension(fw_errstr_len) :: fw_err&
&str__
    interface
        function one_arg(a)
            use fwrap_ktp_mod
            implicit none
            real(kind=fwr_real_8_t), dimension(:, :), intent(inout) :: a
            integer(kind=fwi_integer_t) :: one_arg
        end function one_arg
    end interface
    fw_iserr__ = FW_INIT_ERR__
    fw_ret_arg = one_arg(a)
    fw_iserr__ = FW_NO_ERR__
end subroutine one_arg_c
