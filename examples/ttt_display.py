#!/usr/bin/env python3
"""
ttt_display.py  —  Curses TUI for Aria Tic-Tac-Toe FFI demo

Reads move history from /tmp/aria_ttt.dat (written by ttt_display.c)
and replays all moves with animation in a terminal pseudo-window.

Data format (one line per move):
    <pos> <val>     e.g. "4 1"  (position 4, player X=1)

Board positions:
    0 | 1 | 2
    --+---+--
    3 | 4 | 5
    --+---+--
    6 | 7 | 8

Usage:
    python3 ttt_display.py [/path/to/aria_ttt.dat]
"""

import curses
import time
import sys
import os


LINES = [(0, 1, 2), (3, 4, 5), (6, 7, 8),   # rows
         (0, 3, 6), (1, 4, 7), (2, 5, 8),   # cols
         (0, 4, 8), (2, 4, 6)]               # diags


def read_moves(path):
    moves = []
    try:
        with open(path) as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 2:
                    moves.append((int(parts[0]), int(parts[1])))
    except IOError as e:
        return [], str(e)
    return moves, None


def check_winner(board):
    for a, b, c in LINES:
        if board[a] == board[b] == board[c] != 0:
            return board[a]
    filled = all(v != 0 for v in board)
    return 0 if not filled else 2  # 0=ongoing, 2=draw


def draw_board(scr, board, title, move_info="", winner_msg=""):
    scr.clear()
    h, w = scr.getmaxyx()

    SYM = {0: ' ', 1: 'X', -1: 'O'}
    COLOR_X   = curses.color_pair(1)   # bright yellow
    COLOR_O   = curses.color_pair(2)   # bright cyan
    COLOR_WIN = curses.color_pair(3)   # bright green + bold
    COLOR_HDR = curses.color_pair(4)   # white bold

    BY = max(0, h // 2 - 7)
    BX = max(0, w // 2 - 12)

    # ── Header ──────────────────────────────────────────────────────────
    hdr = "  Aria Lang FFI  —  Tic-Tac-Toe  "
    try:
        scr.addstr(BY, BX, hdr, COLOR_HDR | curses.A_BOLD | curses.A_UNDERLINE)
    except curses.error:
        pass

    # ── Move info ───────────────────────────────────────────────────────
    if move_info:
        try:
            scr.addstr(BY + 1, BX, move_info)
        except curses.error:
            pass

    # ── Board grid ──────────────────────────────────────────────────────
    ROW_Y = BY + 3
    cells = [SYM[board[i]] for i in range(9)]

    def cell_attr(i):
        v = board[i]
        if v == 1:  return COLOR_X | curses.A_BOLD
        if v == -1: return COLOR_O | curses.A_BOLD
        return curses.A_DIM

    def draw_row(y, c0, c1, c2):
        try:
            scr.addstr(y,   BX,     " ")
            scr.addstr(y,   BX + 1, cells[c0], cell_attr(c0))
            scr.addstr(y,   BX + 2, " | ")
            scr.addstr(y,   BX + 5, cells[c1], cell_attr(c1))
            scr.addstr(y,   BX + 6, " | ")
            scr.addstr(y,   BX + 9, cells[c2], cell_attr(c2))
        except curses.error:
            pass

    draw_row(ROW_Y,     0, 1, 2)
    try:
        scr.addstr(ROW_Y + 1, BX, "--+---+--")
    except curses.error:
        pass
    draw_row(ROW_Y + 2, 3, 4, 5)
    try:
        scr.addstr(ROW_Y + 3, BX, "--+---+--")
    except curses.error:
        pass
    draw_row(ROW_Y + 4, 6, 7, 8)

    # ── Position legend ─────────────────────────────────────────────────
    try:
        scr.addstr(ROW_Y + 6, BX, " 0 | 1 | 2    positions", curses.A_DIM)
        scr.addstr(ROW_Y + 7, BX, " 3 | 4 | 5", curses.A_DIM)
        scr.addstr(ROW_Y + 8, BX, " 6 | 7 | 8", curses.A_DIM)
    except curses.error:
        pass

    # ── Winner message ──────────────────────────────────────────────────
    if winner_msg:
        try:
            scr.addstr(ROW_Y + 10, BX, winner_msg, COLOR_WIN | curses.A_BOLD)
            scr.addstr(ROW_Y + 11, BX, " Press any key to exit...", curses.A_DIM)
        except curses.error:
            pass

    scr.refresh()


def run(scr, datfile):
    curses.curs_set(0)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_YELLOW,  -1)   # X
    curses.init_pair(2, curses.COLOR_CYAN,    -1)   # O
    curses.init_pair(3, curses.COLOR_GREEN,   -1)   # winner
    curses.init_pair(4, curses.COLOR_WHITE,   -1)   # header

    moves, err = read_moves(datfile)
    if err or not moves:
        try:
            scr.addstr(0, 0, f"Error reading {datfile}: {err}")
            scr.refresh()
            time.sleep(3)
        except curses.error:
            pass
        return

    board = [0] * 9
    player_name = {1: "X", -1: "O"}

    for i, (pos, val) in enumerate(moves):
        board[pos] = val
        p = player_name.get(val, "?")
        info = f"  Move {i+1}: {p} plays position {pos}"
        draw_board(scr, board, "", info)
        time.sleep(0.85)

    # Final state — determine winner
    result = check_winner(board)
    if result == 1:
        wmsg = "  *** X WINS! ***"
    elif result == -1:
        wmsg = "  *** O WINS! ***"
    elif result == 2:
        wmsg = "  *** DRAW ***"
    else:
        wmsg = "  *** GAME INCOMPLETE ***"

    draw_board(scr, board, "", "  -- Final board --", wmsg)
    scr.getch()


def main():
    datfile = sys.argv[1] if len(sys.argv) > 1 else "/tmp/aria_ttt.dat"
    if not os.path.exists(datfile):
        print(f"Error: data file not found: {datfile}", file=sys.stderr)
        sys.exit(1)
    curses.wrapper(run, datfile)


if __name__ == "__main__":
    main()
