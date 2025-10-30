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



# Prints ShapeEditor state from data starting at index idx. Returns the index at which the next structure starts.
def print_ShapeEditor_state(data: str, version: List[int], idx: int) -> int:
    if version == [1, 0, 0]:

        number_points = struct.unpack('<I', data[idx:idx+4])[0]
        idx = print_mv(data, '\tNumber of points saved: ', '<I', idx)

        for i in range(number_points):
            print(f'\tPoint {i}:')
            idx = print_mv(data, f'\t\tx-position: ', '<f', idx)
            idx = print_mv(data, f'\t\ty-position: ', '<f', idx)
            idx = print_mv(data, f'\t\ty-curve center: ', '<f', idx)
            idx = print_mv(data, f'\t\tinterpolation mode: ', '<I', idx)
            idx = print_mv(data, f'\t\tomega: ', '<f', idx)
            print()
        return idx
        
    else:
        raise ValueError(f'Unknown version: {version}')

# Prints state of the modulation links of an Envelope from data starting at index idx. Returns the index at which the next structure starts.
def print_modulation_state(data: str, version: List[int], idx: int) -> int:
    if version == [1, 0, 0]:

        number_links = struct.unpack('<I', data[idx:idx+4])[0]
        idx = print_mv(data, '\tNumber of modulation links of this Envelope: ', '<I', idx)

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
    if version == [1, 0, 0]:
        idx = print_mv(data, '\tActive mode: ', '<I', idx)
        idx = print_mv(data, '\tmode: ', '<I', idx)
        idx = print_mv(data, '\t\tvalue: ', 'd', idx)
        idx = print_mv(data, '\tmode: ', '<I', idx)
        idx = print_mv(data, '\t\tvalue: ', 'd', idx)
        print()
        return idx

    else:
        raise ValueError(f'Unknown version: {version}')

# Prints all EnvelopeManager states from data starting at index idx. Returns the index at which the next structure starts.
# States include editor states, same as for ShapeEditors, states of modulation links and states of the FrequencyPanels.
def print_EnvelopeManager_state(data: str, version: List[int], idx: int) -> int:
    hline_dashed = '- '*25

    if version == [1, 0, 0]:
        number_envelopes = struct.unpack('<I', data[idx:idx+4])[0]
        print(f'Number of active Envelopes: {number_envelopes}\n')

        idx += 4

        for i in range(number_envelopes):
            print(hline_dashed)
            print(f'Envelope {i}:\n')
            idx = print_ShapeEditor_state(data, version, idx)
            idx = print_modulation_state(data, version, idx)

        print(hline_dashed)
        print('FrequencyPanel states:\n')
        for i in range(number_envelopes):
            print(f'Panel {i}:\n')
            idx = print_FrequencyPanel_state(data, version, idx)

        return idx + 4
        
    else:
        raise ValueError(f'Unknown version: {version}')
    
# Prints the state of the first UDShaper instance in the file in path.
# State includes:
#   - shapeEditor1 & shapeEditor2 states
#   - states of all atcive Envelopes:
#       - Envelope editor state
#       - Envelope modulation links
#       - FrequencyPanel states
def print_UDShaper_state(path: str):
    with open(path, 'rb') as file:
        data = file.read()

    # In FL preset and project files, the data starts 47 bits after the end of the file location
    idx = data.find(b'UDShaper.clap') + 47

    if idx == -1:
        raise Exception('Plugin data not found.')

    version = []
    for i in range(3):
        version.append(struct.unpack('<I', data[idx:idx+4])[0])
        idx += 4

    print(f'UDShaper data found at byte {idx}.')
    print(f'Version {version[0]}.{version[1]}.{version[2]}\n')
    idx = print_mv(data, 'Current distortion mode: ', '<I', idx)
    print()

    hline = '-'*50

    print(hline)
    print('ShapeEditor1:\n')
    idx = print_ShapeEditor_state(data, version, idx)

    print(hline)
    print('ShapeEditor2:\n')
    idx = print_ShapeEditor_state(data, version, idx)

    print(hline)
    print('EnvelopeManager:\n')
    idx = print_EnvelopeManager_state(data, version, idx)
    print(hline)

    if not data[idx:]:
        print('End of file.')
    else:
        print(f'remaining data:\n{len(data)-idx} bytes')


if __name__ =='__main__':
    print_UDShaper_state('test_presets/test1.fst')
