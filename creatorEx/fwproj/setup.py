from fwrap.fwrap_setup import setup, fwrap_cmdclass, configuration

cfg_args = {'extra_sources': ['/home/josh/Desktop/fFwrapEx/creatorEx/source.f90'], 'libraries': [], 'library_dirs': [], 'extra_objects': []}

cfg = configuration(projname='fwproj', **cfg_args)
setup(log='fwrap_setup.log', cmdclass=fwrap_cmdclass, configuration=cfg)
