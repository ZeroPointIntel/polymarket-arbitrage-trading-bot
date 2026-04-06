"""
Async retry decorator with exponential backoff and jitter.

Usage:

    from utils.retry import async_retry, RetryError

    @async_retry(max_attempts=4, base_delay=1.0, max_delay=30.0, exceptions=(httpx.HTTPError,))
    async def fetch_data() -> dict:
        ...

If all attempts fail, RetryError is raised with the last exception attached
as __cause__.
"""

import asyncio
import functools
import logging
import random
import time
from typing import Callable, Optional, Tuple, Type

logger = logging.getLogger(__name__)


class RetryError(Exception):
    """Raised when all retry attempts are exhausted."""

    def __init__(self, message: str, attempts: int, last_exception: Optional[BaseException] = None):
        super().__init__(message)
        self.attempts = attempts
        self.last_exception = last_exception


def _compute_delay(
    attempt: int,
    base_delay: float,
    max_delay: float,
    exponential_base: float = 2.0,
    jitter: bool = True,
) -> float:
    """
    Full-jitter exponential backoff:
        delay = random(0, min(max_delay, base_delay * exponential_base ** attempt))

    Full jitter prevents thundering-herd when many coroutines retry simultaneously.
    """
    cap = min(max_delay, base_delay * (exponential_base ** attempt))
    return random.uniform(0, cap) if jitter else cap


def async_retry(
    max_attempts: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exponential_base: float = 2.0,
    jitter: bool = True,
    exceptions: Tuple[Type[BaseException], ...] = (Exception,),
    on_retry: Optional[Callable[[int, BaseException, float], None]] = None,
) -> Callable:
    """
    Decorator factory — wraps an async function with retry logic.

    Args:
        max_attempts:     Total number of tries (including the first call).
        base_delay:       Starting sleep time in seconds.
        max_delay:        Upper cap for the sleep time in seconds.
        exponential_base: Multiplier applied to base_delay each attempt.
        jitter:           Add full random jitter to prevent thundering-herd.
        exceptions:       Tuple of exception types that trigger a retry.
                          Other exceptions propagate immediately.
        on_retry:         Optional callback(attempt, exc, delay) called before
                          each sleep.  Useful for logging custom messages.

    Raises:
        RetryError: When all attempts fail.
        Any exception NOT in `exceptions` propagates without retrying.

    Example:
        @async_retry(max_attempts=5, base_delay=0.5, exceptions=(httpx.TransportError,))
        async def call_api() -> dict:
            return await client.get("/endpoint")
    """
    if max_attempts < 1:
        raise ValueError("max_attempts must be >= 1")

    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        async def wrapper(*args, **kwargs):
            last_exc: Optional[BaseException] = None

            for attempt in range(max_attempts):
                try:
                    return await func(*args, **kwargs)
                except exceptions as exc:
                    last_exc = exc
                    remaining = max_attempts - attempt - 1

                    if remaining == 0:
                        # Last attempt — do not sleep, raise immediately
                        break

                    delay = _compute_delay(
                        attempt,
                        base_delay=base_delay,
                        max_delay=max_delay,
                        exponential_base=exponential_base,
                        jitter=jitter,
                    )

                    if on_retry is not None:
                        on_retry(attempt + 1, exc, delay)
                    else:
                        logger.warning(
                            "%s failed (attempt %d/%d): %s — retrying in %.2fs",
                            func.__qualname__,
                            attempt + 1,
                            max_attempts,
                            exc,
                            delay,
                        )

                    await asyncio.sleep(delay)

            raise RetryError(
                f"{func.__qualname__} failed after {max_attempts} attempt(s): {last_exc}",
                attempts=max_attempts,
                last_exception=last_exc,
            ) from last_exc

        return wrapper

    return decorator


def sync_retry(
    max_attempts: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exponential_base: float = 2.0,
    jitter: bool = True,
    exceptions: Tuple[Type[BaseException], ...] = (Exception,),
) -> Callable:
    """
    Synchronous counterpart of async_retry.  Identical behaviour but uses
    time.sleep() instead of asyncio.sleep().

    Use this for non-async functions (e.g., healthcheck connectivity checks).
    """
    if max_attempts < 1:
        raise ValueError("max_attempts must be >= 1")

    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            last_exc: Optional[BaseException] = None

            for attempt in range(max_attempts):
                try:
                    return func(*args, **kwargs)
                except exceptions as exc:
                    last_exc = exc
                    remaining = max_attempts - attempt - 1

                    if remaining == 0:
                        break

                    delay = _compute_delay(
                        attempt,
                        base_delay=base_delay,
                        max_delay=max_delay,
                        exponential_base=exponential_base,
                        jitter=jitter,
                    )
                    logger.warning(
                        "%s failed (attempt %d/%d): %s — retrying in %.2fs",
                        func.__qualname__,
                        attempt + 1,
                        max_attempts,
                        exc,
                        delay,
                    )
                    time.sleep(delay)

            raise RetryError(
                f"{func.__qualname__} failed after {max_attempts} attempt(s): {last_exc}",
                attempts=max_attempts,
                last_exception=last_exc,
            ) from last_exc

        return wrapper

    return decorator
