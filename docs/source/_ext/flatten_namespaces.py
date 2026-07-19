"""
Sphinx extension: flattens the namespace scaffolding breathe emits for a file.

Doxygen records a file's namespaces as a flat list of fully qualified names, but breathe rebuilds them into a nested
tree and renders one box per component. A single struct in spp::asts therefore arrives wrapped in two nested boxes, each
adding a level of indentation, and namespaces that a file only forward-declares into show up as empty boxes.

This rewrites each nested chain into one "namespace spp::asts" heading followed by that namespace's contents as
siblings, so nothing is indented and namespaces holding nothing are dropped entirely.
"""

from __future__ import annotations

from typing import NamedTuple

from docutils import nodes
from sphinx import addnodes
from sphinx.application import Sphinx


def _signature(desc: addnodes.desc) -> addnodes.desc_signature | None:
    for child in desc.children:
        if isinstance(child, addnodes.desc_signature):
            return child
    return None


def _content(desc: addnodes.desc) -> addnodes.desc_content | None:
    for child in desc.children:
        if isinstance(child, addnodes.desc_content):
            return child
    return None


def _is_namespace(node: nodes.Node) -> bool:
    """A namespace is the only C++ construct breathe renders with a leading "namespace" keyword."""
    if not isinstance(node, addnodes.desc) or node.get("domain") != "cpp":
        return False
    signature = _signature(node)
    return signature is not None and signature.astext().startswith("namespace ")


def _name(desc: addnodes.desc) -> str:
    signature = _signature(desc)
    if signature is not None:
        for child in signature.findall(addnodes.desc_name):
            return child.astext()
    return ""


class Group(NamedTuple):
    """One namespace worth of output: its genindex anchors, the heading itself, then its contents."""

    namespace: addnodes.desc
    qualified: str
    anchors: list[nodes.Node]
    entities: list[nodes.Node]


def _collect(desc: addnodes.desc, prefix: list[str]) -> list[Group]:
    """Walk a namespace tree, returning one group per namespace that actually contains something."""
    qualified = prefix + [_name(desc)]
    content = _content(desc)

    # Each entity is preceded by its own index node. Those are collected as they are passed and attached to whatever
    # follows, so a nested namespace's genindex entry travels with it rather than being stranded on the parent.
    own: list[nodes.Node] = []
    nested: list[tuple[list[nodes.Node], addnodes.desc]] = []
    pending: list[nodes.Node] = []

    for child in content.children if content is not None else []:
        if isinstance(child, addnodes.index):
            pending.append(child)
        elif _is_namespace(child):
            nested.append((pending, child))
            pending = []
        else:
            own.extend(pending)
            own.append(child)
            pending = []
    own.extend(pending)

    # A namespace whose only children were nested namespaces contributes no heading of its own. That covers the ones a
    # file merely forward-declares into, whose declarations breathe drops because they are defined elsewhere.
    substantial = any(not isinstance(c, addnodes.index) for c in own)
    collected = [Group(desc, "::".join(qualified), [], own)] if substantial else []

    for anchors, child in nested:
        groups = _collect(child, qualified)
        if groups:
            groups[0] = groups[0]._replace(anchors=anchors + groups[0].anchors)
        collected.extend(groups)
    return collected


def _heading(desc: addnodes.desc, qualified: str) -> addnodes.desc:
    """Reuse the namespace's own signature, renamed to the fully qualified form, with no content to indent."""
    signature = _signature(desc)
    assert signature is not None

    for name_node in signature.findall(addnodes.desc_name):
        # The signature keeps its original ids, so existing cross-references and permalinks still resolve.
        name_node.clear()
        name_node += addnodes.desc_sig_name(qualified, qualified)

    heading = addnodes.desc()
    heading.update_all_atts(desc)
    heading += signature
    heading += addnodes.desc_content()
    return heading


def flatten(app: Sphinx, doctree: nodes.document) -> None:
    for desc in list(doctree.findall(_is_namespace)):
        # Only the outermost namespace of a chain is rewritten; _collect handles everything below it. By the time the
        # traversal reaches a nested one it has already been detached, so it no longer has a parent.
        parent = desc.parent
        if parent is None or any(_is_namespace(a) for a in _ancestors(desc)):
            continue

        replacement: list[nodes.Node] = []
        for group in _collect(desc, []):
            replacement.extend(group.anchors)
            replacement.append(_heading(group.namespace, group.qualified))
            replacement.extend(group.entities)

        index = parent.index(desc)
        parent[index:index + 1] = replacement


def _ancestors(node: nodes.Node):
    node = node.parent
    while node is not None:
        yield node
        node = node.parent


def setup(app: Sphinx) -> dict[str, object]:
    # doctree-read runs after breathe has rendered, but before Sphinx resolves references against the tree.
    app.connect("doctree-read", flatten)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
