# ============================================================================
# SageMips Example: Bubble Sort (Sage)
# Compile: sagemips compile examples/sage/bubble_sort.sage
# ============================================================================

proc bubble_sort(arr):
    let n = len(arr)
    let i = 0
    while i < n:
        let j = 0
        while j < n - i - 1:
            if arr[j] > arr[j + 1]:
                let tmp = arr[j]
                arr[j] = arr[j + 1]
                arr[j + 1] = tmp
            j = j + 1
        i = i + 1

let data = [64, 34, 25, 12, 22, 11, 90, 5, 42, 7]
print "Before: " + str(data)
bubble_sort(data)
print "After:  " + str(data)
