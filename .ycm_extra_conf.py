def Settings(**kwargs):
    return {
        'flags': [
            '-x', 'c++',

            '-std=c++17',

            '-Wall',
            '-Wno-unused-function',
            '-Wno-bugprone-sizeof-expression',
        ],
    }
