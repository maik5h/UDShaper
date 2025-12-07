# Code to print a binary file that contains the state of UDShaper in human readable format.

import os
import struct
from typing import List

# Prints text, followed by the four bytes of data after idx interpreted as format. Returns index moved by the number of bytes expected.
def print_mv(data: str, text: str, format: str, idx: int) -> int:
    # Get the buffer size from given format.
    if format == '<I' or format == '<f':
        buffer = 4
    elif format == 'd':
        buffer = 8
    else:
        raise ValueError(f'Unknown format: {format}')

    print(text + f'{struct.unpack(format, data[idx:idx+buffer])[0]}')

    return idx + buffer

# Checks if there are any LFOs linked to the previous ModulatedParameter.
# Prints them if there are any, does nothig if not.
def print_mod_state(data: str, idx: int) -> int:
  number_links = struct.unpack('<I', data[idx:idx+4])[0]
  idx += 4
  if number_links > 0:
      link_indices = []
      for _ in range(number_links):
          link_indices.append(struct.unpack('<I', data[idx:idx+4])[0])
          idx += 4
      print(f'\t\t\tconnected to links: {link_indices}\n')
  return idx


# Prints ShapeEditor state from data starting at index idx. Returns the index at which the next structure starts.
def print_ShapeEditor_state(data: str, version: List[int], idx: int) -> int:
    if version == 0:

        number_points = struct.unpack('<I', data[idx:idx+4])[0]
        idx = print_mv(data, '\tNumber of points saved: ', '<I', idx)

        for i in range(number_points):
            print(f'\tPoint {i}:')
            idx = print_mv(data, f'\t\tinterpolation mode: ', '<I', idx)
            idx = print_mv(data, f'\t\tx-position: ', '<f', idx)
            idx = print_mod_state(data, idx)
            idx = print_mv(data, f'\t\ty-position: ', '<f', idx)
            idx = print_mod_state(data, idx)
            idx = print_mv(data, f'\t\tcurve center position: ', '<f', idx)
            idx = print_mod_state(data, idx)
            idx = print_mv(data, f'\t\tomega: ', '<f', idx)
            print()
        return idx
        
    else:
        raise ValueError(f'Unknown version: {version}')

# Prints state of the modulation links of an LFO from data starting at index idx. Returns the index at which the next structure starts.
def print_modulation_state(data: str, version: List[int], idx: int) -> int:
    if version == 0:

        number_links = struct.unpack('<I', data[idx:idx+4])[0]
        idx = print_mv(data, '\tNumber of modulation links of this LFO: ', '<I', idx)

        for i in range(number_links):
            print(f'\tLink {i}:')
            idx = print_mv(data, '\t\tShapeEditor: ', '<I', idx)
            idx = print_mv(data, '\t\tPoint idx: ', '<I', idx)
            idx = print_mv(data, '\t\tmode: ', '<I', idx)
            idx = print_mv(data, '\t\tmodulation amount: ', '<f', idx)
            print()
        if number_links == 0:
            print()
        return idx

    else:
        raise ValueError(f'Unknown version: {version}')

# Prints state of one instance of FrequencyPanel from data starting at index idx. Returns the index at which the next structure starts.
def print_FrequencyPanel_state(data: str, version: List[int], idx: int) -> int:
    if version == 0:
        idx = print_mv(data, '\tActive mode: ', '<I', idx)
        idx = print_mv(data, '\tmode: ', '<I', idx)
        idx = print_mv(data, '\t\tvalue: ', 'd', idx)
        idx = print_mv(data, '\tmode: ', '<I', idx)
        idx = print_mv(data, '\t\tvalue: ', 'd', idx)
        print()
        return idx

    else:
        raise ValueError(f'Unknown version: {version}')

# Prints all LFO states from data starting at index idx. Returns the index at which the next structure starts.
# States include editor states, same as for ShapeEditors, states of modulation links and states of the FrequencyPanels.
def print_LFO_state(data: str, version: List[int], idx: int) -> int:
    hline_dashed = '- '*25

    if version == 0:
        number_LFOs = struct.unpack('<I', data[idx:idx+4])[0]
        print(f'Number of active LFOs: {number_LFOs}\n')

        idx += 4

        for i in range(number_LFOs):
            print(hline_dashed)
            print(f'LFO {i}:\n')
            idx = print_ShapeEditor_state(data, version, idx)
            # idx = print_modulation_state(data, version, idx)

        print(hline_dashed)
        # print('FrequencyPanel states:\n')
        # for i in range(number_LFOs):
        #     print(f'Panel {i}:\n')
        #     idx = print_FrequencyPanel_state(data, version, idx)

        return idx + 4
        
    else:
        raise ValueError(f'Unknown version: {version}')
    
# Prints the state of the first UDShaper instance in the file in path.
# State includes:
#   - shapeEditor1 & shapeEditor2 states
#   - states of all atcive LFOs:
#   - iPlug2 internal parameters, such as distortion mode, active LFO, modulation amplitudes, ...
def print_UDShaper_state(path: str):
    with open(path, 'rb') as file:
        data = file.read()

    # In FL preset and project files, the data starts 47 bits after the end of the file location
    idx = data.find(b'UDShaper.clap') + 47

    if idx == -1:
        raise Exception('Plugin data not found.')

    print('Header:')
    print(data[:idx])

    print('data:')
    print(data[idx:])

    version = struct.unpack('<I', data[idx:idx+4])[0]
    idx += 4

    print(f'UDShaper data found at byte {idx}.')
    print(f'remaining bytes: {len(data) - idx}')
    print(f'Version {version}\n')

    hline = '-'*50

    print(hline)
    print('ShapeEditor1:\n')
    idx = print_ShapeEditor_state(data, version, idx)

    print(hline)
    print('ShapeEditor2:\n')
    idx = print_ShapeEditor_state(data, version, idx)

    print(hline)
    print('LFO:\n')
    idx = print_LFO_state(data, version, idx)
    print(hline)

    # I dont know what happens in the following bytes.

    #print('iPlug2 internally saved parameters:')
    #idx = print_mv(data, 'Current distortion mode: ', 'd', idx)
    #print()

    #idx = print_mv(data, 'Active LFO index: ', 'd', idx)
    #print()

    idx += 132

    # Skip frequency parameters for now
    idx += 30 * 4

    # These bytes match again

    print('Modulation amounts:')
    mod_amps = []
    for i in range(10):
        mod_amps.clear()
        for j in range(10):
            amount = struct.unpack('d', data[idx:idx+8])[0]
            mod_amps.append(f'{amount:1.2f}')
            idx += 8
        print(mod_amps)

    if not data[idx:]:
        print('End of file.')
    else:
        print(f'remaining data:\n{len(data)-idx} bytes')


if __name__ =='__main__':
    print_UDShaper_state('../test_presets/test_vector_1.fst')
