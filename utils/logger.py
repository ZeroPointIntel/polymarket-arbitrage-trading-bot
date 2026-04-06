"""
Standardized logging setup for the Polymarket Latency Arbitrage Bot.
Provides a consistent logger with both console and file handlers.
"""

import logging
import sys
from logging.handlers import RotatingFileHandler
from typing import Optional



BANNER = r"""
  ██████╗  ██████╗ ██╗  ██╗   ██╗███╗   ███╗ █████╗ ██████╗ ██╗  ██╗███████╗████████╗
  ██╔══██╗██╔═══██╗██║  ╚██╗ ██╔╝████╗ ████║██╔══██╗██╔══██╗██║ ██╔╝██╔════╝╚══██╔══╝
  ██████╔╝██║   ██║██║   ╚████╔╝ ██╔████╔██║███████║██████╔╝█████╔╝ █████╗     ██║
  ██╔═══╝ ██║   ██║██║    ╚██╔╝  ██║╚██╔╝██║██╔══██║██╔══██╗██╔═██╗ ██╔══╝    ██║
  ██║     ╚██████╔╝███████╗██║   ██║ ╚═╝ ██║██║  ██║██║  ██║██║  ██╗███████╗  ██║
  ╚═╝      ╚═════╝ ╚══════╝╚═╝   ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝  ╚═╝

  by Genoshide  |  polymarket arbitrage script bot
"""


class ColorFormatter(logging.Formatter):
    """Adds ANSI colour codes to console log output for readability."""

    GREY = "\x1b[38;20m"
    CYAN = "\x1b[36;20m"
    YELLOW = "\x1b[33;20m"
    RED = "\x1b[31;20m"
    BOLD_RED = "\x1b[31;1m"
    RESET = "\x1b[0m"

    FORMATS = {
        logging.DEBUG: GREY + "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s" + RESET,
        logging.INFO: CYAN + "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s" + RESET,
        logging.WARNING: YELLOW + "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s" + RESET,
        logging.ERROR: RED + "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s" + RESET,
        logging.CRITICAL: BOLD_RED + "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s" + RESET,
    }

    def format(self, record: logging.LogRecord) -> str:
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt, datefmt="%Y-%m-%d %H:%M:%S")
        return formatter.format(record)


def get_logger(
    name: str,
    level: str = "INFO",
    log_file: Optional[str] = None,
    max_bytes: int = 10 * 1024 * 1024,  # 10 MB
    backup_count: int = 5,
) -> logging.Logger:
    """
    Create and return a configured logger instance.

    Args:
        name: Logger name (typically __name__ of the calling module).
        level: Logging level string (DEBUG, INFO, WARNING, ERROR, CRITICAL).
        log_file: Optional path to a rotating log file.
        max_bytes: Maximum size of each log file before rotation.
        backup_count: Number of rotated log files to retain.

    Returns:
        A configured logging.Logger instance.
    """
    logger = logging.getLogger(name)

    # Avoid adding duplicate handlers on re-import
    if logger.handlers:
        return logger

    numeric_level = getattr(logging, level.upper(), logging.INFO)
    logger.setLevel(numeric_level)

    # Console handler with colour
    # Use sys.stdout directly — wrapping sys.stdout.buffer in a new TextIOWrapper
    # causes stdout to be closed when setup_logging() strips the handler.
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(numeric_level)
    console_handler.setFormatter(ColorFormatter())
    logger.addHandler(console_handler)

    # File handler (plain text, no colour codes)
    if log_file:
        file_handler = RotatingFileHandler(
            log_file,
            maxBytes=max_bytes,
            backupCount=backup_count,
            encoding="utf-8",
        )
        file_handler.setLevel(numeric_level)
        file_formatter = logging.Formatter(
            "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
        file_handler.setFormatter(file_formatter)
        logger.addHandler(file_handler)

    return logger


def _strip_stream_handlers(logger: logging.Logger) -> None:
    """Remove all StreamHandlers from a logger (leaves file handlers intact)."""
    for h in logger.handlers[:]:
        if isinstance(h, logging.StreamHandler) and not isinstance(h, RotatingFileHandler):
            logger.removeHandler(h)


def setup_logging(log_file: str = "polymarket_bot.log", log_level: str = "INFO") -> None:
    """
    Add a rotating file handler to the root logger (file-only, idempotent).
    Console StreamHandlers are left intact here so startup errors remain
    visible in the terminal.  Call disable_console_logging() once the
    Rich Live dashboard takes over the screen.
    """
    # Silence noisy HTTP libraries — only show WARNING+
    for noisy in ("httpx", "httpcore", "hpack", "asyncio", "urllib3"):
        logging.getLogger(noisy).setLevel(logging.WARNING)

    numeric_level = getattr(logging, log_level.upper(), logging.INFO)
    root = logging.getLogger()

    has_file_handler = any(isinstance(h, RotatingFileHandler) for h in root.handlers)
    if not has_file_handler:
        if root.level == logging.NOTSET or root.level > numeric_level:
            root.setLevel(numeric_level)
        fh = RotatingFileHandler(
            log_file,
            maxBytes=10 * 1024 * 1024,
            backupCount=5,
            encoding="utf-8",
        )
        fh.setLevel(numeric_level)
        fh.setFormatter(logging.Formatter(
            "%(asctime)s [%(levelname)-8s] %(name)s: %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        ))
        root.addHandler(fh)


def disable_console_logging() -> None:
    """
    Strip every StreamHandler from every logger so that log records never
    reach the terminal and disrupt the Rich Live dashboard.
    Called once, right before the Live context starts rendering.
    """
    root = logging.getLogger()
    _strip_stream_handlers(root)
    for child in logging.Logger.manager.loggerDict.values():
        if isinstance(child, logging.Logger):
            _strip_stream_handlers(child)


def print_banner() -> None:
    """Print the bot startup banner to stdout."""
    try:
        print(BANNER, flush=True)
    except UnicodeEncodeError:
        # Windows terminal using CP1252 — write directly to stdout byte buffer
        sys.stdout.buffer.write(BANNER.encode("utf-8", errors="replace") + b"\n")
        sys.stdout.flush()
