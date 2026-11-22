"""Background jobs so the window stays responsive."""
from PySide6.QtCore import QObject, QRunnable, Signal, Slot


class _Signals(QObject):
    done = Signal(int, object)         # job id, result (whatever the job function returned)
    failed = Signal(int, str)
    progress = Signal(int, str, float)


class Job(QRunnable):
    """Runs fn(progress_callback) on the thread pool."""

    def __init__(self, job_id, fn):
        super().__init__()
        self.job_id, self.fn = job_id, fn
        self.signals = _Signals()

    @Slot()
    def run(self):
        try:
            result = self.fn(lambda m, p: self.signals.progress.emit(self.job_id, m, p))
            self.signals.done.emit(self.job_id, result)
        except Exception as exc:                  # noqa: BLE001 - shown to the user
            self.signals.failed.emit(self.job_id, str(exc))
