from fwrap.fwrap_setup import setup, fwrap_cmdclass, configuration

cfg_args = {'extra_sources': ['/home/josh/Desktop/fFwrapEx/creatorEx/source.f90'], 'libraries': ['gfortran'], 'library_dirs': ['/usr/lib/gcc/x86_linux-gnu/4.8.2'], 'extra_objects': []}

cfg = configuration(projname='one_arg', **cfg_args)
setup(log='fwrap_setup.log', cmdclass=fwrap_cmdclass, configuration=cfg)
