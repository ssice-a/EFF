"""Read verified blocks from an already linked format-2 directory package."""
from .resource_file import digest
from .type_tree import require


def read_payload(package, row):
    resource = package["_resources"][row["file"]]
    data = resource["blocks"][row["name"]]
    require(len(data) == row["size"] and digest(data) == row["sha256"], "resource block correspondence")
    return data
