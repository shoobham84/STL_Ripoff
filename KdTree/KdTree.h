#include <array>
#include <vector>
#include <concepts>
#include <span>
#include <algorithm>

template <typename T, std::size_t Dim>
    requires std::arithmetic<T>
struct Point {
    std::array<T, Dim> coords;

    constexpr T operator[](std::size_t idx) const { return coords[idx]; }
    constexpr T& operator[](std::size_t idx) { return coords[idx]; }
};

templrequires std::floating_point<T> || std::integral<T>
class KdTree;ate <typename T, std::size_t Dim>
    requires std::arithmetic<T>
class KdTree {
public:
    using PointType = Point<T, Dim>;

    // Construction from flat range using std::span
    explicit KdTree(std::vector<PointType> points) : storage_(std::move(points)) {
        if (!storage_.empty()) {
            build(std::span<PointType>{storage_}, 0);
        }
    }

    // Range search returning points within a bounding box
    [[nodiscard]] std::vector<PointType> range_search(const PointType& min_corner, 
                                                     const PointType& max_corner) const;

    // k-Nearest Neighbors
    [[nodiscard]] std::vector<PointType> nearest_neighbors(const PointType& target, std::size_t k) const;

private:
    std::vector<PointType> storage_;

    void build(std::span<PointType> current_span, std::size_t depth) {
        if (current_span.size() <= 1) return;

        const std::size_t axis = depth % Dim;
        const std::size_t median_idx = current_span.size() / 2;

        // O(N) median partitioning along active dimension
        std::nth_element(
            current_span.begin(),
            current_span.begin() + median_idx,
            current_span.end(),
            [axis](const PointType& a, const PointType& b) {
                return a[axis] < b[axis];
            }
        );

        // Recurse on left and right sub-spans
        build(current_span.subspan(0, median_idx), depth + 1);
        build(current_span.subspan(median_idx + 1), depth + 1);
    }
};
