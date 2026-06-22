#pragma once

/// @file status_line.hpp
/// @brief Terminal status line that overwrites itself in place.

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/// Renders a set of named columns as a single terminal line that is
/// overwritten on each call to render().
///
/// Columns are registered once with add() and updated between renders with
/// set(). Each column grows to accommodate the widest value it has ever
/// held, so the layout never shrinks mid-run. Padding clears any characters
/// left over from a previously longer line.
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
        if (value.size() > col.min_width) {
            col.min_width = value.size();
        }
        col.value = std::move(value);
    }

    /// Overwrites the current terminal line with all columns formatted as
    /// @c "label: value", separated by two spaces.
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
        if (line.size() < m_prev_len) {
            line.append(m_prev_len - line.size(), ' ');
        }
        m_prev_len = line.size();
        std::cout << '\r' << line << std::flush;
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
};
