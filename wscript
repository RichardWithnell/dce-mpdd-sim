## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

import ns3waf

def configure(conf):
    ns3waf.check_modules(conf, ['core', 'internet', 'dce', 'point-to-point',
                                'mobility', 'wifi', 'applications','csma'],
                    mandatory = True)

def build(bld):
    bld.build_a_script('dce', needed = ['core',
                                        'internet',
                                        'dce',
                                        'point-to-point',
                                        'mobility', 'wifi', 'applications'],
				  target='bin/dce-mpdd-flat',
				  source=['dce-mpdd-flat.cc'],
				  )
    bld.build_a_script('dce', needed = ['core',
                                    'internet',
                                    'dce',
                                    'point-to-point',
                                    'mobility', 'wifi', 'applications'],
              target='bin/dce-mpdd-nested',
              source=['dce-mpdd-nested.cc'],
              )
    bld.build_a_script('dce', needed = ['core',
                                'internet',
                                'dce',
                                'point-to-point', 'csma',
                                'mobility', 'wifi', 'applications'],
          target='bin/dce-mpdd-nested-csma',
          source=['dce-mpdd-nested-csma.cc'],
          )
    bld.build_a_script('dce', needed = ['core',
                            'internet',
                            'dce',
                            'point-to-point', 'csma',
                            'mobility', 'wifi', 'applications'],
      target='bin/csma-test',
      source=['csma-test.cc'],
      )
