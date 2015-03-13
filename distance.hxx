#include <vector>
#include <stdint.h>

template<class T>
uint_fast32_t edit_distance(const T &a, const T &b)
{
    const size_t lenA = a.size();
    const size_t lenB = b.size();

    std::vector<uint_fast32_t> col(lenB+1);
    std::vector<uint_fast32_t> prevCol(lenB+1);

    for (uint_fast32_t i = 0; i < prevCol.size(); i++)
    {
        prevCol[i] = i;
    }

    for (uint_fast32_t i = 0; i < lenA; i++)
    {
        col[0] = i + 1;

        for (uint_fast32_t j = 0; j < lenB; j++)
        {
            col[j + 1] = std::min(
                    std::min(
                        prevCol[1 + j] + 1,
                        col[j] + 1),
                    prevCol[j] + (a[i]==b[j] ? 0 : 1));
        }
        col.swap(prevCol);
    }
    return prevCol[lenB];
}
