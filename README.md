# `printf_binary` plugin for FreeBSD printf implementation
=============

FreeBSD `printf` implementation lets you extend the `printf` by registering
the extension with `register_printf_render` function. My extension adds `%b`
specifier, which lets you print the variable in binary `0` and `1` format.
