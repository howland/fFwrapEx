function one_arg(a)
  implicit none
  !Automatic type discovery: the C type correspondacnes
  !will be automatically determined at compile time
  integer :: one_arg
  real(kind=8), dimension(:, : ), intent(inout) :: a

  a = 1.61803399_8
  one_arg = 42

end function
