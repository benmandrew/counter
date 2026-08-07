#pragma once

/// @file status_line.hpp
/// @brief Terminal status line that overwrites itself in place.

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

/// True when stdout is a terminal.
///
/// Cached because the answer cannot change during a run and the progress path
/// asks once per render. Everything that moves the cursor has to consult this:
/// a redirected run keeps every frame it ever drew, escape codes and all, so
/// what overwrites itself on screen becomes tens of kilobytes of noise in a
/// log. scripts/run_experiments.py redirects every run to <output-dir>/run.log,
/// which is the common case rather than the exception.
inline bool stdout_is_tty() {
    static const bool is_tty = isatty(STDOUT_FILENO) != 0;
    return is_tty;
}

/// Renders a set of named columns as a single terminal line that is
/// overwritten on each call to render().
///
/// Columns are registered once with add() and updated between renders with
/// set(). Each column grows to accommodate the widest value it has ever
/// held, so the layout never shrinks mid-run.
///
/// When the rendered content is wider than the terminal and wraps onto
/// multiple rows, render() queries the terminal width via TIOCGWINSZ,
/// moves the cursor back to the first row of the previous output, and
/// uses @c \\033[J to erase to end of screen before reprinting.
///
/// Off a terminal, render() draws nothing and finish() alone emits the line,
/// so a redirected run gets exactly one committed line per finish() and no
/// escape codes at all.
///
/// A column may be declared @em transient, meaning it belongs to the live
/// display but not to the committed line. A within-unit percentage is the
/// motivating case: it is the whole point of the line while the unit runs, and
/// always reads 100% by the time the line is committed.
///
/// Typical use:
/// @code
///   StatusLine s;
///   auto col_gen  = s.add("gen");
///   auto col_pct  = s.add("%", true);   // live only
///   auto col_time = s.add("time");
///   s.set(col_gen, "1/20");
///   s.set(col_time, "0.12s");
///   s.render();   // \r gen: 1/20  %: 40%  time: 0.12s
///   s.finish();   // commits "gen: 1/20  time: 0.12s" and a newline
/// @endcode
class StatusLine {
   public:
    /// Registers a new column with the given label and returns its index.
    /// A @p transient column shows in render() but not in finish().
    std::size_t add(std::string label, bool transient = false) {
        const std::size_t idx = m_cols.size();
        m_cols.push_back({std::move(label), "", 0, transient});
        return idx;
    }

    /// Updates the value of column @p idx.
    /// The column's display width grows to fit the widest value seen.
    void set(std::size_t idx, std::string value) {
        Column& col = m_cols[idx];
        col.min_width = std::max(value.size(), col.min_width);
        col.value = std::move(value);
    }

    /// Overwrites the previous render with all columns formatted as
    /// @c "label: value", separated by two spaces.
    /// Moves the cursor back to the start of the previous output block
    /// (accounting for terminal wrapping) and erases to end of screen
    /// before printing.
    /// Does nothing off a terminal, where finish() commits the line instead.
    void render() {
        if (!stdout_is_tty()) {
            return;
        }
        const std::string line = format_line(true);
        clear_previous();
        std::cout << line << std::flush;
        m_prev_len = line.size();
    }

    /// Commits the line: prints it without its transient columns, followed by
    /// a newline, and resets the length tracker. On a terminal this overwrites
    /// whatever render() last drew, so the transient columns leave no trace.
    void finish() {
        std::string line = format_line(false);
        // The per-cell padding exists to stop columns jittering between
        // renders; on a committed line it is only trailing whitespace.
        line.erase(line.find_last_not_of(' ') + 1);
        if (stdout_is_tty()) {
            clear_previous();
        }
        std::cout << line << '\n';
        m_prev_len = 0;
    }

   private:
    struct Column {
        std::string label;
        std::string value;
        std::size_t min_width{0};
        bool transient{false};
    };
    std::vector<Column> m_cols;
    std::size_t m_prev_len{0};

    [[nodiscard]] std::string format_line(bool include_transient) const {
        std::string line;
        for (const Column& col : m_cols) {
            if (col.transient && !include_transient) {
                continue;
            }
            if (!line.empty()) {
                line += "  ";
            }
            const std::size_t cell_width = col.label.size() + 2 + col.min_width;
            const std::string cell = col.label + ": " + col.value;
            line += cell;
            if (cell.size() < cell_width) {
                line.append(cell_width - cell.size(), ' ');
            }
        }
        return line;
    }

    void clear_previous() const {
        const std::size_t prev_rows = rows_for(m_prev_len, terminal_width());
        if (prev_rows > 1) {
            std::cout << "\033[" << (prev_rows - 1) << "A\r\033[J";
        } else if (prev_rows == 1) {
            std::cout << "\r\033[J";
        }
    }

    static std::size_t terminal_width() {
        struct winsize win{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 && win.ws_col > 0) {
            return static_cast<std::size_t>(win.ws_col);
        }
        return 0;
    }

    // Returns the number of terminal rows occupied by `len` characters.
    // Returns 0 for an empty previous render (nothing to clear).
    // Falls back to 1 row when the terminal width is unknown.
    static std::size_t rows_for(std::size_t len, std::size_t width) {
        if (len == 0) {
            return 0;
        }
        if (width == 0) {
            return 1;
        }
        return (len + width - 1) / width;
    }
};
