#!/usr/bin/env python3
"""Check that the three hand-maintained descriptions of the TOML config agree.

The config surface is spelled out in three places, and nothing in the compiler
ties them together:

  * ``config_key_spec()`` in ``src/config_io.cpp`` -- the keys the parser
    recognises. A key missing here is reported as "unknown key" at run time
    even though the parser reads it.
  * ``schemas/config-schema.json`` -- what an editor validates against. It sets
    ``additionalProperties: false``, so a key missing here is flagged as an
    error in the editor even though the binary accepts it.
  * ``example-config.toml`` -- the annotated template users copy from.

Drift between them is silent and has happened: ``nsga2-replicate`` was added to
the parser and the docs but not to the schema enum. This compares all three and
exits non-zero on any disagreement.

It does not, and cannot cheaply, catch a key added to an ``apply_*`` function
and nowhere else -- that needs reflection the language does not offer. It does
mean such a key cannot reach the schema or the template unnoticed.
"""

import argparse
import json
import re
import sys
import tomllib
from pathlib import Path

# Keys whose accepted values are a closed set, checked against the schema's
# "enum". Names are unique across sections, so the section need not be given.
ENUM_KEYS = ("selection_scheme", "metric", "repair_mode", "status_grading")


# --- src/config_io.cpp -------------------------------------------------------


def tokenise(source):
    return re.findall(r'"[^"]*"|[A-Za-z_][A-Za-z_0-9]*|[(){},]', source)


class SpecParser:
    """Recursive-descent parser for the section(...) literal in config_io.cpp.

    The literal is pure data -- section({keys}, {{"name", section(...)}, ...})
    -- so a parser small enough to read beats a regex that half-works.
    """

    def __init__(self, tokens):
        self.toks = tokens
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None

    def take(self, expected=None):
        tok = self.peek()
        if tok is None:
            raise ValueError(f"unexpected end of the section(...) literal")
        if expected is not None and tok != expected:
            raise ValueError(f"expected {expected!r} at token {self.i}, got {tok!r}")
        self.i += 1
        return tok

    def parse_section(self):
        self.take("section")
        self.take("(")
        keys = self.parse_string_list()
        tables = {}
        if self.peek() == ",":
            self.take(",")
            tables = self.parse_table_map()
        self.take(")")
        return keys, tables

    def parse_string_list(self):
        self.take("{")
        keys = set()
        while self.peek() != "}":
            keys.add(self.take()[1:-1])
            if self.peek() == ",":
                self.take(",")
        self.take("}")
        return keys

    def parse_table_map(self):
        self.take("{")
        tables = {}
        while self.peek() == "{":
            self.take("{")
            name = self.take()[1:-1]
            self.take(",")
            tables[name] = self.parse_section()
            self.take("}")
            if self.peek() == ",":
                self.take(",")
        self.take("}")
        return tables


def flatten(spec, prefix=""):
    """Turn a (keys, tables) tree into a set of dotted paths."""
    keys, tables = spec
    paths = {prefix + k for k in keys}
    for name, sub in tables.items():
        paths.add(prefix + name)
        paths |= flatten(sub, prefix + name + ".")
    return paths


def parser_keys(source):
    start = source.find("const KeySpec& config_key_spec()")
    if start < 0:
        raise ValueError("config_key_spec() not found")
    body = source[source.index("section(", start) : source.index("return spec;", start)]
    return flatten(SpecParser(tokenise(body)).parse_section())


def parser_enums(source):
    """Accepted string values per key, read from the *val == "..." chains."""
    found = {}
    for match in re.finditer(r'tbl\["(\w+)"\]\.value<std::string>\(\)', source):
        key = match.group(1)
        rest = source[match.end() :]
        # The chain ends at the next key lookup, or at the end of the file.
        stop = re.search(r'tbl\["\w+"\]', rest)
        block = rest[: stop.start()] if stop else rest
        found[key] = set(re.findall(r'\*val == "([^"]+)"', block))
    return found


# --- schemas/config-schema.json ----------------------------------------------


def schema_keys(properties, prefix=""):
    paths = set()
    for name, node in properties.items():
        paths.add(prefix + name)
        if "properties" in node:
            paths |= schema_keys(node["properties"], prefix + name + ".")
    return paths


def schema_nodes(properties, prefix=""):
    nodes = {}
    for name, node in properties.items():
        nodes[prefix + name] = node
        if "properties" in node:
            nodes.update(schema_nodes(node["properties"], prefix + name + "."))
    return nodes


def open_objects(schema, prefix="(root)"):
    """Objects missing additionalProperties: false, which validate anything."""
    offenders = []
    if schema.get("type") == "object" and schema.get("additionalProperties") is not False:
        offenders.append(prefix)
    for name, node in schema.get("properties", {}).items():
        offenders += open_objects(node, f"{prefix}.{name}" if prefix != "(root)" else name)
    return offenders


# --- example-config.toml -----------------------------------------------------


def toml_keys(table, prefix=""):
    paths = set()
    for name, value in table.items():
        paths.add(prefix + name)
        if isinstance(value, dict):
            paths |= toml_keys(value, prefix + name + ".")
    return paths


def toml_scalars(table, prefix=""):
    """Leaf (non-table) keys and their values, by dotted path."""
    values = {}
    for name, value in table.items():
        if isinstance(value, dict):
            values.update(toml_scalars(value, prefix + name + "."))
        else:
            values[prefix + name] = value
    return values


def toml_values(table, prefix=""):
    values = {}
    for name, value in table.items():
        if isinstance(value, dict):
            values.update(toml_values(value, prefix + name + "."))
        elif isinstance(value, str):
            values[prefix + name] = value
    return values


# --- include/config.hpp ------------------------------------------------------

# TOML path -> the Config member holding its default. Keys absent here are not
# value-checked: genetic.selection_scheme, tlsf.repair_mode and
# model_counting.metric are enums already covered by the enum check above, and
# runtime.parallel defaults to hardware concurrency, which is not a constant to
# pin. Everything else must appear, so a new key cannot quietly skip the check.
DEFAULT_FIELDS = {
    "genetic.generations": "generations",
    "genetic.population_size": "population_size",
    "genetic.selection_rate": "selection_rate",
    "genetic.elitism_rate": "elitism_rate",
    "genetic.crossover_rate": "crossover_rate",
    "genetic.mutation_rate": "mutation_rate",
    "fitness.weight_syntactic": "fitness_weight_syntactic",
    "fitness.weight_semantic": "fitness_weight_semantic",
    "fitness.weight_status": "fitness_weight_status",
    "mutation.p_trigger": "p_trigger",
    "mutation.p_response": "p_response",
    "mutation.p_timing": "p_timing",
    "mutation.p_add_assumption": "p_add_assumption",
    "mutation.p_remove_guarantee": "p_remove_guarantee",
    "mutation.p_conditional_assumption": "p_conditional_assumption",
    "mutation.strengthen_assumptions": "strengthen_assumptions",
    "mutation.allow_output_assumptions": "allow_output_assumptions",
    "tlsf.muc_max_iterations": "muc_max_iterations",
    "tlsf.mutation.p_assumption": "tlsf_p_assumption",
    "tlsf.mutation.p_temporal": "tlsf_p_temporal",
    "model_counting.default_bound": "default_model_counting_bound",
    "filters.run_weakening": "run_weakening_filter",
    "filters.run_implication": "run_implication_filter",
    "filters.run_vacuity": "run_vacuity_filter",
    "filters.run_well_separation": "run_well_separation_filter",
    "runtime.black_timeout_ms": "black_timeout",
    "runtime.ltlsynt_timeout_ms": "ltlsynt_timeout",
    "runtime.ltl2tgba_timeout_ms": "ltl2tgba_timeout",
    "runtime.ltlfilt_timeout_ms": "ltlfilt_timeout",
    "runtime.ganak_timeout_ms": "ganak_timeout",
    "runtime.max_concurrent_realizability": "max_concurrent_realizability",
    "runtime.max_scoring_failure_rate": "max_scoring_failure_rate",
    "runtime.dashboard": "dashboard",
}

# Keys the parser accepts but that carry no pinnable constant default.
UNPINNED_KEYS = {
    "genetic.selection_scheme",
    "tlsf.repair_mode",
    "model_counting.metric",
    "fitness.status_grading",
    "runtime.parallel",
}


def hpp_defaults(source):
    """Default value per Config member, from its in-class initialiser.

    Handles both spellings the struct uses: ``T name = value;`` for the scalars
    and ``std::chrono::milliseconds name{value};`` for the tool timeouts. C++
    digit separators are stripped so 10'000 reads as 10000.

    Comment lines are dropped first. ``[^;]+`` spans newlines, so prose of the
    form ``p = 0.0002`` in a doc comment matches through to the next semicolon
    and eats the initialiser that follows it -- which is a member documenting
    its own measured default breaking the check on that default.
    """
    body = source[source.index("struct Config {") :]
    body = body[: body.index("\n};")]
    body = "\n".join(
        line for line in body.splitlines() if not line.lstrip().startswith("//")
    )
    found = {}
    for name, value in re.findall(r"(\w+)\s*=\s*([^;]+);", body):
        found[name] = value.strip()
    for name, value in re.findall(r"(\w+)\{([^}]*)\}\s*;", body):
        found.setdefault(name, value.strip())
    return {k: v.replace("'", "") for k, v in found.items()}


def as_comparable(value):
    """Normalise a C++ literal or a TOML value to compare the two."""
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return float(value)
    text = str(value).strip()
    if text in ("true", "false"):
        return text == "true"
    try:
        return float(text)
    except ValueError:
        return text


# --- checks ------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="repository root (default: the parent of scripts/)",
    )
    root = ap.parse_args().root

    cpp = root / "src" / "config_io.cpp"
    hpp = root / "include" / "config.hpp"
    schema_path = root / "schemas" / "config-schema.json"
    example_path = root / "example-config.toml"

    # A file that will not load is a failure of this check, not a crash of it:
    # report it the same way as a mismatch so the lint output stays readable.
    def load(path, loader):
        try:
            return loader(path.read_text(encoding="utf-8")), None
        except FileNotFoundError:
            return None, f"{path} does not exist."
        except (OSError, ValueError) as exc:
            return None, f"{path}: {exc}"

    cpp_source, cpp_error = load(cpp, lambda text: text)
    hpp_source, hpp_error = load(hpp, lambda text: text)
    schema, schema_error = load(schema_path, json.loads)
    example, example_error = load(example_path, tomllib.loads)

    from_parser, spec_error = (None, None)
    if cpp_source is not None:
        try:
            from_parser = parser_keys(cpp_source)
        except ValueError as exc:
            spec_error = f"{cpp}: could not read config_key_spec(): {exc}"

    fatal = [
        e
        for e in (cpp_error, hpp_error, schema_error, example_error, spec_error)
        if e
    ]
    if fatal:
        print("Config key parity check could not run:\n", file=sys.stderr)
        for error in fatal:
            print(f"  - {error}", file=sys.stderr)
        return 1

    assert cpp_source is not None and schema is not None and example is not None
    assert from_parser is not None and hpp_source is not None

    from_schema = schema_keys(schema["properties"])
    from_example = toml_keys(example)

    errors = []

    for path in sorted(from_parser - from_schema):
        errors.append(
            f"{schema_path.name}: missing '{path}', which the parser accepts. "
            f"Editors validating against the schema will reject it."
        )
    for path in sorted(from_schema - from_parser):
        errors.append(
            f"config_io.cpp: config_key_spec() is missing '{path}', which the "
            f"schema declares. The parser will warn 'unknown key' on it."
        )
    for path in sorted(from_example - from_parser):
        errors.append(
            f"{example_path.name}: sets '{path}', which the parser does not "
            f"recognise and will warn about."
        )

    # example-config.toml is the template users copy, and it says every value in
    # it is the built-in default. That was silently false for eight keys, so the
    # claim is now enforced: each scalar key's value must equal the initialiser
    # of the Config member that backs it.
    defaults = hpp_defaults(hpp_source)
    for path, value in sorted(toml_scalars(example).items()):
        if path in UNPINNED_KEYS:
            continue
        field = DEFAULT_FIELDS.get(path)
        if field is None:
            errors.append(
                f"check_config_schema.py: '{path}' is in "
                f"{example_path.name} but not in DEFAULT_FIELDS, so its value "
                f"goes unchecked. Add it (or to UNPINNED_KEYS if it has no "
                f"constant default)."
            )
            continue
        if field not in defaults:
            errors.append(
                f"config.hpp: no initialiser found for '{field}', which backs "
                f"'{path}'."
            )
            continue
        want = as_comparable(defaults[field])
        got = as_comparable(value)
        if want != got:
            errors.append(
                f"{example_path.name}: '{path}' is {got!r}, but config.hpp "
                f"defaults {field} to {want!r}. The template must show the "
                f"built-in default."
            )

    # An open object defeats the point of the schema: it would validate any key.
    for offender in open_objects(schema):
        errors.append(
            f"{schema_path.name}: object '{offender}' does not set "
            f'"additionalProperties": false, so it accepts unknown keys.'
        )

    # Closed-value keys: the parser's accepted strings must match the enum.
    nodes = schema_nodes(schema["properties"])
    enums = parser_enums(cpp_source)
    for key in ENUM_KEYS:
        accepted = enums.get(key)
        if accepted is None:
            errors.append(f"config_io.cpp: no string-valued key '{key}' found.")
            continue
        matches = [(p, n) for p, n in nodes.items() if p.split(".")[-1] == key]
        if len(matches) != 1:
            errors.append(
                f"{schema_path.name}: expected exactly one '{key}' property, "
                f"found {len(matches)}."
            )
            continue
        path, node = matches[0]
        declared = set(node.get("enum", []))
        for value in sorted(accepted - declared):
            errors.append(
                f"{schema_path.name}: '{path}' enum is missing \"{value}\", "
                f"which the parser accepts."
            )
        for value in sorted(declared - accepted):
            errors.append(
                f"{schema_path.name}: '{path}' enum declares \"{value}\", "
                f"which the parser rejects."
            )
        default = node.get("default")
        if default is not None and default not in accepted:
            errors.append(
                f"{schema_path.name}: '{path}' default \"{default}\" is not a "
                f"value the parser accepts."
            )

    # The template must use values the parser accepts, not just valid keys.
    for path, value in sorted(toml_values(example).items()):
        key = path.split(".")[-1]
        if key in ENUM_KEYS and value not in enums.get(key, {value}):
            errors.append(
                f"{example_path.name}: '{path}' is \"{value}\", which the "
                f"parser rejects."
            )

    if errors:
        print("Config key parity check failed:\n", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        print(
            f"\n{len(errors)} problem(s). The parser, the schema, and the "
            f"template must describe the same config.",
            file=sys.stderr,
        )
        return 1

    print(f"Config key parity: {len(from_parser)} keys agree across "
          f"config_io.cpp, {schema_path.name}, and {example_path.name}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
