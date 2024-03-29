/*
 * Copyright (c) 2021-2023 Noah Orensa.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#include <dtest.h>
#include <named_parameters.h>

module("named-parameters")
.dependsOn({
    "exception",
    "hash-map",
    "stream-serializer",
    "std-serialization",
    "json"
});

using namespace spl;

unit("named-parameters", "default-value")
.body([] {
    NamedParameters p;
    p.addParameter("a", "default");
    assert(p.get<std::string>("a") == "default");
});

unit("named-parameters", "set/get")
.body([] {
    NamedParameters p;
    p.addParameter("a", "default");
    p.set("a", "test");
    assert(p.get<std::string>("a") == "test");
    assert(p.isSet("a"));
});

unit("named-parameters", "reset")
.body([] {
    NamedParameters p;
    p.addParameter("a", "default");
    p.set("a", "test");
    assert(p.get<std::string>("a") == "test");
    p.reset("a");
    assert(p.get<std::string>("a") == "default");
    assert(! p.isSet("a"));
});
