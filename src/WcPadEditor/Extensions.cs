using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Wangdera.WcPadEditor
{
    internal static class Extensions
    {
        internal static void AddRange<T>(this ICollection<T> c, IEnumerable<T> items)
        {
            foreach (var item in items)
            {
                c.Add(item); 
            }
        }
    }
}
