from distutils.core import setup, Extension

module1 = Extension('pyfasttextureutils',
                    define_macros = [('MAJOR_VERSION', '1'),
                                     ('MINOR_VERSION', '0')],
                    include_dirs = [],
                    libraries = [],
                    library_dirs = [],
                    sources = ['pyfasttextureutils.c'])

setup (name = 'Python Fast Texture Utils',
       version = '1.0',
       description = 'Functions for modifying images written in C.',
       author = 'LagoLunatic',
       author_email = '',
       url = 'https://github.com/LagoLunatic',
       long_description = '''
Python module written in C for modifying image files with better performance than native Python code.
''',
       ext_modules = [module1])
