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
/// Typical use:
/// @code
///   StatusLine s;
///   auto col_gen  = s.add("gen");
///   auto col_time = s.add("time");
///   s.set(col_gen, "1/20");
///   s.set(col_time, "0.12s");
///   s.render();   // \r gen: 1/20  time: 0.12s
///   s.finish();   // \n — commits the line
/// @endcode
class StatusLine {
   public:
    /// Registers a new column with the given label and returns its index.
    std::size_t add(std::string label) {
        const std::size_t idx = m_cols.size();
        m_cols.push_back({std::move(label), "", 0});
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
    void render() {
        std::string line;
        for (std::size_t i = 0; i < m_cols.size(); ++i) {
            if (i > 0) {
                line += "  ";
            }
            const Column& col = m_cols[i];
            const std::size_t cell_width = col.label.size() + 2 + col.min_width;
            const std::string cell = col.label + ": " + col.value;
            line += cell;
            if (cell.size() < cell_width) {
                line.append(cell_width - cell.size(), ' ');
            }
        }

        const std::size_t prev_rows = rows_for(m_prev_len, terminal_width());
        if (prev_rows > 1) {
            std::cout << "\033[" << (prev_rows - 1) << "A\r\033[J";
        } else if (prev_rows == 1) {
            std::cout << "\r\033[J";
        }

        std::cout << line << std::flush;
        m_prev_len = line.size();
    }

    /// Ends the current line with a newline and resets the length tracker.
    void finish() {
        std::cout << '\n';
        m_prev_len = 0;
    }

   private:
    struct Column {
        std::string label;
        std::string value;
        std::size_t min_width{0};
    };
    std::vector<Column> m_cols;
    std::size_t m_prev_len{0};

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
