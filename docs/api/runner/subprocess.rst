subprocess.hpp
==============

``run_subprocess`` runs an external tool, captures its stdout and stderr
together, and reaps the child. It is the single implementation shared by the
``black``, ``ltlfilt``, ``ganak``, ``ltlsynt`` and ``ltl2tgba`` wrappers.

Each of those wrappers used to carry its own near-identical copy. The copies
differed only in whether they supported a timeout and whether the child had to
die with its parent, so both are options on ``SubprocessOptions`` rather than
reasons to duplicate the code. Keeping one copy matters because every fix to
the spawn path otherwise has to be made once per wrapper.

Children are spawned with ``posix_spawn`` unless ``m_die_with_parent`` is set.
glibc implements ``posix_spawn`` with ``clone(CLONE_VM|CLONE_VFORK)``, which
copies no page tables and does not write-protect the parent — worth having
because the scoring pool spawns from many threads while the rest keep writing.
``m_die_with_parent`` asks for ``prctl(PR_SET_PDEATHSIG, SIGKILL)``, which has
no ``posix_spawn`` equivalent and so forces the ``fork()`` path; only the two
tools that can leave multi-gigabyte orphans use it.

The pipe is opened ``O_CLOEXEC``. Without that a spawned child inherits, and
holds open, the pipes of other calls still in flight, and their readers never
see end of file.

.. doxygenfile:: subprocess.hpp
