#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <map>
#include <ctime>
#include <stdint.h>
#include <cstring>

#include "distance.hxx"

inline void Usage(const std::string &progName);

// Joins elements of container, separated by separator
template <typename T>
inline std::string Join(const T &container, const std::string separator);

// Replaces target with replacement in input 
inline std::string Replace(const std::string &input, const std::string &target, const std::string &replacement);

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        std::string arg(argv[1]);
        if (arg[0] == 'h' || arg[0] == 'H' || arg == "--help" || arg == "-h" || arg == "-?" || arg == "--usage")
        {
            Usage(argv[0]);
            return 1;
        }
    }

    // Time Slice in seconds
    time_t timeSliceSize = 30 * 60;

    // Percent required similarity to consider messages the same
    double pct = 0.8;

    // Time formatter
    std::string timeFormatter("%F %T");

    // Dummy elements
    uint_fast16_t dummyElements = 2;

    char cutoff = '\0';

    if (argc > 1)
    {
        std::istringstream ss(argv[1]);
        double sliceSize;
        ss >> sliceSize;
        // minutes -> seconds
        timeSliceSize = static_cast<uint_fast32_t>(sliceSize * 60.0);
    }

    if (argc > 2)
    {
        std::istringstream ss(argv[2]);
        ss >> pct;
    }

    if (argc > 3)
    {
        timeFormatter.assign(argv[3]);
    }

    if (argc > 4)
    {
        std::istringstream ss(argv[4]);
        ss >> dummyElements;
    }

    if (argc > 5)
    {
        cutoff = argv[5][0];
    }

    if (std::cin)
    {
        // List of messages collected so far
        std::vector<std::vector<std::string> > messages;
        // Map of timeslice -> {map of index -> occurrences}
        std::map<time_t, std::map<uint_fast16_t, uint_fast32_t> > slices;

        time_t currentSlice = 0.0;

        uint_fast64_t lineNum = 0;
        while (!(std::cin.eof() || std::cin.fail()))
        {
            if (lineNum % 10000 == 0)
            {
                std::cerr << "Processing line number " << lineNum << std::endl;
            }
            ++lineNum;

            std::string line;
            std::getline(std::cin, line);

            // Rip the format off the beginning
            tm time;
            memset(&time, '\0', sizeof(time));
            const char * format = strptime(line.c_str(), timeFormatter.c_str(), &time);

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
                if (cutoff == '\0')
                {
                    contentSS.str(format);
                } else
                {
                    std::istringstream content(format);
                    std::string contentString;
                    std::getline(content, contentString, cutoff);
                    contentSS.str(contentString);
                }

                std::string dummy;

                for (uint_fast16_t i = 0; i < dummyElements; ++i)
                {
                    // Dump dummy element
                    contentSS >> dummy;
                }

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

                for (uint_fast32_t i = 0; i < messages.size(); ++i)
                {
                    const uint_fast32_t distance = edit_distance(messages[i], contentVec);
                    if (distance < leastDistance)
                    {
                        leastDistance = distance;
                        closestMatch = 1.0 - (static_cast<double>(distance) / static_cast<double>(contentVec.size()));
                        matchIndex = i;
                    }
                }

                if (closestMatch >= pct)
                {
                    slices[currentSlice][matchIndex] += 1;
                } else
                {
                    slices[currentSlice][messages.size()] += 1;
                    messages.push_back(contentVec);
                }
            }
        }
        // Output messages
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
            size_t size = strftime(buffer, 1024, "%F %T", gmtime(&it->first));
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
    } else
    {
        std::cerr << "Something wrong with std input!" << std::endl;
        Usage(argv[0]);
        return 1;
    }

    return 0;
}

void Usage(const std::string &progName)
{
    std::cout << "USAGE:" << '\n'
        << '\t' << progName << " [{slice size in minutes}] [{pct match required}] [{time formatter for timestamp}] [{count of dummy elements between timestamp and message}] [ending char]" << '\n'
        << "\t\t" << "Analize a log by time slices and its percent matching required in its timestamps" << '\n'
        << "\t\t" << "Logfile taken in through stdin (can be concatenated)" << '\n'
        << "\t\t" << "Default slice size is 30 minutes" << '\n'
        << "\t\t" << "Default pct match is 80% (done by words)." << '\n'
        << "\t\t" << "Default time formatter is \"%F %T\"" << '\n'
        << "\t\t" << "Default dummy elements is 2" << '\n'
        << "\t\t" << "Ending char defines a cutoff for messages.  null is default and disables cutoff." << '\n'
        << "\t\t" << "Outputs csv to standard output." << std::endl;
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

inline std::string Replace(const std::string &input, const std::string &target, const std::string &replacement)
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
