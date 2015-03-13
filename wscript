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
              target='bin/dce-mpdd-nested-wifi',
              source=['dce-mpdd-nested-wifi.cc'],
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
          target='bin/dce-mptcp-subflow64',
          source=['dce-mptcp-subflow64.cc'],
          )
    bld.build_a_script('dce', needed = ['core',
                                'internet',
                                'dce',
                                'point-to-point', 'csma',
                                'mobility', 'wifi', 'applications'],
          target='bin/dce-mpdd-throughput-csma',
          source=['dce-mpdd-throughput-csma.cc'],
          )
    bld.build_a_script('dce', needed = ['core',
                                'internet',
                                'dce',
                                'point-to-point', 'csma',
                                'mobility', 'wifi', 'applications'],
          target='bin/dce-nat-test',
          source=['dce-nat-test.cc'],
          )
