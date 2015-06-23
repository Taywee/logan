#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <stdint.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <pcre.h>

#include "distance.hxx"

inline void Usage(const std::string &progName);

// Joins elements of container, separated by separator
template <typename T>
inline std::string Join(const T &container, const std::string separator);

// Replaces target with replacement in input 
inline std::string Replace(const std::string &input, const std::string &target, const std::string &replacement);

inline size_t Hash(const std::string &string);

inline size_t Hash(const std::vector<std::string>& vec);

enum Output
{
    CSV,
    REPORT
};

int main(int argc, char **argv)
{
    Output output = CSV;

    // Time Slice in seconds
    time_t timeSliceSize = 30 * 60;

    // Percent required similarity to consider messages the same
    double pct = 0.8;

    // Time formatter
    std::string timeFormatter("%F %T");
    std::string matchPattern("^.+$");
    std::string replacePattern("${0}");

    // Dummy elements
    uint_fast32_t minimum = 1;

    int opt;
    while ((opt = getopt(argc, argv, "hs:o:p:f:d:m:r:R:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                {
                    Usage(argv[0]);
                    return 0;
                    break;
                }

            case 's':
                {
                    std::istringstream ss(optarg);
                    double sliceSize;
                    ss >> sliceSize;
                    // minutes -> seconds
                    timeSliceSize = static_cast<uint_fast32_t>(sliceSize * 60.0);
                    break;
                }

            case 'm':
                {
                    std::istringstream ss(optarg);
                    ss >> minimum;
                    break;
                }

            case 'o':
                {
                    if (strlen(optarg) > 0)
                    {
                        switch (tolower(optarg[0]))
                        {
                            case 'r':
                                {
                                    output = REPORT;
                                    break;
                                }
                            case 'c':
                                {
                                    output = CSV;
                                    break;
                                }
                        }
                    }
                    break;
                }

            case 'p':
                {
                    std::istringstream ss(optarg);
                    ss >> pct;
                    break;
                }

            case 'f':
                {
                    timeFormatter.assign(optarg);
                    break;
                }

            case 'r':
                {
                    matchPattern.assign(optarg);
                    break;
                }

            case 'R':
                {
                    replacePattern.assign(optarg);
                    break;
                }

            case '?':
                {
                    Usage(argv[0]);
                    return (1);
                }
        }
    }

    // These pieces are interleaved evenly.  There is always one more
    // replacepart as replaceref.  The pattern begins and ends with a
    // replacepart.
    std::vector<std::string> replaceparts;
    std::vector<long> replacerefs;

    int erroffset;
    const char *error;

    // Break down the replace string into a quickly-buildable representation for output
    {
        std::string capturePattern = "\\$\\{(\\d+)\\}";
        pcre *re = pcre_compile(capturePattern.c_str(), 0, &error, &erroffset, NULL);
        int ovector[6];

        // If the capture pattern is found in the replace data
        while (pcre_exec(re, NULL, replacePattern.data(), replacePattern.length(), 0, 0, ovector, 6) > 0)
        {
            // Extract the number from the capture
            std::string number(replacePattern.substr(ovector[2], ovector[3] - ovector[2]));
            replacerefs.push_back(std::strtol(number.c_str(), NULL, 10));

            // Push back the first part of the string
            replaceparts.push_back(replacePattern.substr(0, ovector[0]));
            // Erase Everything up to and including the pattern place.
            replacePattern.erase(0, ovector[1]);
        }
        // Everything left of replace is the tail.
        replaceparts.push_back(replacePattern);
        pcre_free(re);
    }

    // Replace all dollar sign escapes
    for (std::vector<std::string>::iterator it = replaceparts.begin(); it != replaceparts.end(); ++it)
    {
        std::string &replacepart = *it;

        size_t pos = 0;
        while (pos < replacepart.size())
        {
            size_t loc = replacepart.find('$', pos);
            if (loc == replacepart.npos)
            {
                break;
            } else
            {
                if (((loc + 1) < replacepart.size()) && (replacepart.at(loc + 1) == '$'))
                {
                    replacepart.erase(loc, 1);
                    pos = loc + 1;
                } else
                {
                    std::cerr << "Replace is wrong.  All dollar signs should only escape a capture replace or another dollar sign" << std::endl;
                    return 1;
                }
            }
        }
    }

    // Compile match pattern
    pcre *re = pcre_compile(matchPattern.c_str(), 0, &error, &erroffset, NULL);
    if (!re)
    {
        std::cerr << "PCRE compile error: " << error << std::endl;
        return 1;
    }

    // JIT compile pattern and do other stuff
    error = NULL;
    pcre_extra *study = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &error);
    if (error)
    {
        std::cerr << "PCRE Study error: " << error << std::endl;
        return 1;
    }

    int captures;
    int rc = pcre_fullinfo(re, study, PCRE_INFO_CAPTURECOUNT, &captures);
    
    // Allocate data for substring storage.
    // Each pair of subsequent ints are bounds for a beginning and end of
    // subcapture, with the first pair being the entire pattern.
    std::vector<int> ovector((captures + 1) * 3);

    // List of messages collected so far
    std::vector<std::vector<std::string> > messages;
    // Hashes of messages collected, if looking for exact matches
    std::vector<size_t> hashes;

    // Map of timeslice -> {map of index -> occurrences}
    std::map<time_t, std::map<uint_fast16_t, uint_fast32_t> > slices;

    time_t currentSlice = time(NULL);
    time_t latestSlice = currentSlice - timeSliceSize;

    uint_fast64_t lineNum = 0;
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (lineNum % 10000 == 0)
        {
            std::cerr << "Processing line number " << lineNum << std::endl;
        }
        ++lineNum;

        // Try to match the pattern.  If it's found, do the replace.
        rc = pcre_exec(re, study, line.data(), line.length(), 0, 0, ovector.data(), ovector.size());
        if (rc > 0)
        {
            std::stringstream lineBuild;

            for (unsigned int i = 0; i < replacerefs.size(); ++i)
            {
                const int replaceref = replacerefs[i];

                // Ovector is interleaved in pairs
                const int start = ovector[replaceref * 2];
                const int end = ovector[(replaceref * 2) + 1];

                lineBuild << replaceparts[i] << line.substr(start, end - start);
            }

            lineBuild << replaceparts.back();
            line.assign(lineBuild.str());
        }

        // Rip the format off the beginning
        tm time;
        memset(&time, '\0', sizeof(time));
        const char * format = strptime(line.c_str(), timeFormatter.c_str(), &time);
        // Determine DST automatically
        time.tm_isdst = -1;

        // Skip the line if the format is not found
        if (format)
        {
            const time_t timestamp = mktime(&time);

            if (currentSlice == 0.0)
            {
                currentSlice = timestamp;
            } else
            {
                while (timestamp < currentSlice)
                {
                    currentSlice -= timeSliceSize;
                }
                while ((timestamp - currentSlice) >= timeSliceSize)
                {
                    currentSlice += timeSliceSize;
                }
            }

            //std::istringstream contentSS(format);
            std::istringstream contentSS;

            contentSS.str(format);

            // Break down contentVec into its parts
            std::vector<std::string> contentVec;
            while (!contentSS.eof())
            {
                std::string item;
                contentSS >> item;
                contentVec.push_back(item);
            }

            uint_fast16_t leastDistance = contentVec.size();
            double closestMatch = 0.0;
            uint_fast32_t matchIndex = 0;
            size_t contentHash = Hash(contentVec);

            // Iterate through existing messages
            for (uint_fast32_t i = 0; i < messages.size(); ++i)
            {
                // If we aren't checking for exact equality
                if (pct < 0.99)
                {
                    // Compute the edit distance
                    const uint_fast32_t distance = edit_distance(messages[i], contentVec);

                    // Iteratively try to find the closest match.  This unfortunately involves iterating through all messages
                    if (distance < leastDistance)
                    {
                        leastDistance = distance;
                        closestMatch = 1.0 - (static_cast<double>(distance) / static_cast<double>(contentVec.size()));
                        matchIndex = i;
                    }
                } else
                {
                    // If we're looking for equality, we just need equality
                    if (hashes[i] == contentHash)
                    {
                        // Set closestMatch to be pct to instantly pass the later check
                        closestMatch = pct;
                        matchIndex = i;

                        // Because we need equality, the first match is good enough
                        break;
                    }
                }
            }

            if (closestMatch >= pct)
            {
                slices[currentSlice][matchIndex] += 1;
            } else
            {
                slices[currentSlice][messages.size()] += 1;
                messages.push_back(contentVec);
                hashes.push_back(Hash(contentVec));
            }
        }
    }

    switch (output)
    {
        case CSV:
            {
                std::cout << "Time";
                for (uint_fast32_t i = 0; i < messages.size(); ++i)
                {
                    std::vector<std::string> &message = messages[i];
                    std::cout << ",\"" << Replace(Join(message, " "), "\"", "\"\"") << "\"";
                }
                std::cout << '\n';
                char buffer[1024];
                for (std::map<time_t, std::map<uint_fast16_t, uint_fast32_t> >::iterator it = slices.begin(); it != slices.end(); ++it)
                {
                    size_t size = strftime(buffer, 1024, "%F %T", localtime(&it->first));
                    if (size)
                    {
                        std::cout.write(buffer, size);
                    } else
                    {
                        std::cout.write(buffer, 1024);
                    }
                    for (uint_fast32_t i = 0; i < messages.size(); ++i)
                    {
                        std::cout << ',' << it->second[i];
                    }
                    std::cout << '\n';
                }
                break;
            }

        case REPORT:
            {
                char buffer[1024];
                size_t size = strftime(buffer, 1024, "%F %T", localtime(&latestSlice));
                std::cout << "Time starting: " << std::string(buffer, size) << "\n";

                time_t endTime = latestSlice + timeSliceSize;
                size = strftime(buffer, 1024, "%F %T", localtime(&endTime));
                std::cout << "Time ending: " << std::string(buffer, size) << "\n\n";

                std::map<uint_fast16_t, uint_fast32_t> &slice = slices[latestSlice];

                uint_fast32_t total = 0;
                for (uint_fast32_t i = 0; i < messages.size(); ++i)
                {
                    if (slice[i] >= minimum)
                    {
                        total += slice[i];
                        std::vector<std::string> &message = messages[i];
                        std::cout << Join(message, " ") << ":\n";
                        std::cout << "\t" << slice[i] << "\n\n";
                    }
                }

                std::cout << "TOTAL:\n";
                std::cout << "\t" << total << std::endl;

                break;
            }
    }

    return 0;
}

void Usage(const std::string &progName)
{
    std::cout << "USAGE:" << '\n'
        << '\t' << progName << " [options...]" << '\n'
        << "\t\t-s ##\t" << "Slice size in minutes.  Default 30" << '\n'
        << "\t\t-o [type]\t" << "Output type.  CSV is default.  Options: Report, CSV" << '\n'
        << "\t\t-m [minimum]\t" << "Minimum matches needed for Report.  Default is 1." << '\n'
        << "\t\t-p ##\t" << "pct match required (in a ratio 0 < p < 1).  Default is 0.8" << '\n'
        << "\t\t-f [format]\t" << "Time formatter. Default is \"%F %T\"" << '\n'
        << "\t\t-r [pattern]\t" << "PCRE regex pattern, used in conjunction with -R to warp input to a proper format.  Default is \"^.+$\"" << '\n'
        << "\t\t-R [replace]\t" << "Regex replacement, $$ is a literal dollar sign, and ${n} inserts the n'th capture group. \"${0}\" is default" << '\n'
        << "\t\t-h\t" << "Show this help menu." << '\n'
        << '\n'
        << "\t\t" << "Takes messages from standard input (order is unimportant)." << '\n'
        << "\t\t" << "Outputs csv to standard output." << '\n'
        << "\t\t" << "This expects to find the timestamp first.  If it is not first, use sed/awk or something to fix it." << std::endl;
}

template <typename T>
std::string Join(const T &container, const std::string separator)
{
    std::ostringstream oss;
    for (typename T::const_iterator it = container.begin(); it != container.end(); ++it)
    {
        if (it != container.begin())
        {
            oss << separator;
        }
        oss << *it;
    }
    return oss.str();
}

std::string Replace(const std::string &input, const std::string &target, const std::string &replacement)
{
    std::string output(input);
    size_t pos = 0;
    while((pos = output.find(target, pos)) != output.npos)
    {
        output.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }
    return output;
}

size_t Hash(const std::string& string)
{
    size_t hash = 0;
    for (uint_fast32_t i = 0; i < string.size(); ++i)
    {
        hash = string[i] + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

size_t Hash(const std::vector<std::string>& vec)
{
    size_t seed = 0;
    for (uint_fast32_t i = 0; i < vec.size(); ++i)
    {
        seed ^= Hash(vec[i]) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }
    return seed;
}
