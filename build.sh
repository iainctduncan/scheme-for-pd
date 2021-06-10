#!/bin/bash

killall pd
make && make install && /Applications/Pd-0.51-1.app/Contents/Resources/Scripts/../bin/pd patchers/s4pd-test.pd patchers/s4pd-test.pd

