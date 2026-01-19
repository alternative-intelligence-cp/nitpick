# Struct Pointers

**Category**: Types → Structs  
**Purpose**: Pointers to structs

---

## Declaration

```aria
struct Node {
    data: i32,
    next: *Node  // Pointer to same type
}

// Create
node: Node = { data = 42, next = nil };

// Pointer to struct
ptr: *Node = &node;
```

---

## Accessing via Pointer

```aria
// Auto-dereference with dot
value: i32 = ptr.data;  // No need for ->

// Modify through pointer
ptr.data = 100;
```

---

## Linked Structures

```aria
// Linked list
struct ListNode {
    value: i32,
    next: *ListNode
}

fn create_list() -> *ListNode {
    head: *ListNode = aria_alloc(ListNode);
    head.value = 1;
    head.next = nil;
    return head;
}

// Traverse
fn print_list(head: *ListNode) {
    current: *ListNode = head;
    while current != nil {
        stdout << current.value;
        current = current.next;
    }
}
```

---

## Tree Structures

```aria
struct TreeNode {
    data: i32,
    left: *TreeNode,
    right: *TreeNode
}

fn create_tree() -> *TreeNode {
    root: *TreeNode = aria_alloc(TreeNode);
    root.data = 10;
    root.left = nil;
    root.right = nil;
    return root;
}
```

---

## Best Practices

### ✅ DO: Initialize to nil

```aria
node: Node = {
    data = 0,
    next = nil  // Always initialize pointers
};
```

### ✅ DO: Check for nil

```aria
when ptr != nil then
    value: i32 = ptr.data;
end
```

### ❌ DON'T: Dereference Without Checking

```aria
value: i32 = ptr.data;  // ❌ Might crash if nil!
```

---

## Related

- [Structs](struct.md)
- [Pointers](pointers.md)
- [nil](nil_null_void.md)

---

**Remember**: Use `.` to access - **auto-dereferences** pointers!
