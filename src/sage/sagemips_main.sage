# SageMips Main Entry Point (Sage)
# src/sage/sagemips_main.sage
#
# Build:
#   SAGE_PATH=src/sage sage --compile src/sage/sagemips_main.sage -o sagemips_sage
#
# Run:
#   ./sagemips_sage asm examples/mips/hello.s
#   ./sagemips_sage run hello.mips

import cli

proc main():
    return cli.dispatch()

main()
