"""Keep install archives confined to PocketBook application/system storage."""


def validate_reader_layout(names):
    for name in names:
        parts = name.rstrip('/').split('/')
        if (parts[0] not in ('system', 'applications')
                or any(part in ('', '.', '..') for part in parts)
                or '\\' in name):
            raise ValueError('Install ZIP contains a Library-visible or unsafe path: ' + name)
