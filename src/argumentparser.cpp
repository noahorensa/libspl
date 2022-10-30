#include <argumentparser.h>

using namespace spl;

ArgumentParser::ArgumentParser(const std::initializer_list<Argument> &arguments)
:   _args(arguments.size())
{
    for (const auto &a : arguments) _args.put(a._argument, a);
}

ArgumentParser & ArgumentParser::parse(int argc, const char * const *argv) {
    while (argc > 0) {
        if (! _args.contains(argv[0])) {
            throw DynamicMessageError("Unknown argument '", argv[0], "' encountered");
        }

        const auto &arg = _args[argv[0]];

        --argc;
        ++argv;

        if (argc < arg._numParams) {
            throw DynamicMessageError("Insufficient parameters supplied to '", arg._argument, "'");
        }

        if (arg._action && ! arg._action(argv)) {
            throw DynamicMessageError("Error during parsing argument '", arg._argument, "'");
        }

        argc -= arg._numParams;
        argv += arg._numParams;
    }

    return *this;
}
