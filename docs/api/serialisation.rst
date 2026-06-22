serialisation.hpp
=================

JSON serialisation and deserialisation for all core types via
`nlohmann/json <https://github.com/nlohmann/json>`_. Provides ``to_json`` /
``from_json`` overloads for ``Timing`` (tagged union on a ``"type"`` field),
``Formula``, ``Requirement``, and ``Specification``, as well as file-level
helpers ``load_specification`` / ``save_specification`` used by the CLI tools.
The ``timing::`` namespace wrappers ensure ADL resolves the overloads when
visiting a ``std::variant<timing::*>``.

.. doxygennamespace:: serialisation
   :content-only:
