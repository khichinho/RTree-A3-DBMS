#include "file_manager.h"
#include "buffer_manager.h"
#include "constants.h"
#include "errors.h"

#include <bits/stdc++.h>

// Dimensions of a point
int d = 2;

// Number of maximum children in a node (width of the tree at a node)
int maxCap = 10;

int rootIndex = -1;
int focusIndex = 0;
// It stores the number of bytes used by the current node
int NodeSize = 0;
int endPageOffset = 0;

using namespace std;

class MBR
{
public:
    vector<pair<int, int>> mbr;
    MBR()
    {
        this->mbr.resize(d, make_pair(INT_MIN, INT_MIN));
    }
    MBR(int n)
    {
        this->mbr.resize(n, make_pair(INT_MIN, INT_MIN));
    }
    MBR(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->mbr[i].first = m->mbr[i].first;
            this->mbr[i].second = m->mbr[i].second;
        }
    }
};

class Node
{
public:
    int index;
    int parentIndex;
    MBR *bounds;
    vector<int> childIndex;
    vector<MBR *> child_bounds;

    Node()
    {
        this->index = -1;
        this->parentIndex = -1;
        this->bounds = new MBR(d);
        this->childIndex.resize(maxCap);
        this->child_bounds.resize(maxCap);

        for (int i = 0; i < child_bounds.size(); i++)
        {
            this->child_bounds[i] = new MBR(d);
        }
    }

    void setBounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].first = m->mbr[i].first;
            this->bounds->mbr[i].second = m->mbr[i].second;
        }
    }

    void setLeftBounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].first = m->mbr[i].first;
        }
    }

    void setRightBounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].second = m->mbr[i].second;
        }
    }

    bool isLeaf()
    {
        if (this->childIndex[0] != -1)
            return false;
        return true;
    }

    void printNode()
    {
        cout << "---------------------------------------------------" << endl;
        cout << "[*] INDEX: " << this->index << endl
             << endl;
        cout << "[*] PARENT INDEX: " << this->parentIndex << endl;
        cout << "[*] BOUNDS: ";
        for (int i = 0; i < d; i++)
        {
            cout << "(" << this->bounds->mbr[i].first << "," << this->bounds->mbr[i].second << ") ";
        }
        cout << "\n[*] CHILDREN: ";
        for (int i = 0; i < maxCap; i++)
        {
            cout << this->childIndex[i] << " ";
        }
        cout << "--------------------------------------------------" << endl;
    }
};

void deleteMBR(MBR *m)
{
    m->mbr.clear();
    m->mbr.shrink_to_fit();
    delete (m);
}

void deleteNode(Node *node)
{
    deleteMBR(node->bounds);
    node->childIndex.clear();
    node->childIndex.shrink_to_fit();
    for (int i = 0; i < node->child_bounds.size(); i++)
    {
        deleteMBR(node->child_bounds[i]);
    }
    node->child_bounds.clear();
    node->child_bounds.shrink_to_fit();
    delete (node);
}

// store node in char* data
Node *extract_node(char *data)
{
    Node *node = new Node();
    // assign all values to node variables
    node->index = *(int *)(data);
    data += sizeof(int);
    node->parentIndex = *(int *)(data);
    data += sizeof(int);
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        node->bounds->mbr[i].first = *(int *)(data);
        data += sizeof(int);
    }
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        node->bounds->mbr[i].second = *(int *)(data);
        data += sizeof(int);
    }

    for (int i = 0; i < node->childIndex.size(); i++)
    {
        node->childIndex[i] = *(int *)(data);
        data += sizeof(int);
        for (int j = 0; j < d; j++)
        {
            node->child_bounds[i]->mbr[j].first = *(int *)(data);
            data += sizeof(int);
        }
        for (int j = 0; j < d; j++)
        {
            node->child_bounds[i]->mbr[j].second = *(int *)(data);
            data += sizeof(int);
        }
    }
    return node;
}

// This function is to copy data from node to the memory
void copy_data(Node *node, char *data)
{
    memcpy(data, &node->index, sizeof(int));
    data += sizeof(int);
    memcpy(data, &node->parentIndex, sizeof(int));
    data += sizeof(int);
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        memcpy(data, &node->bounds->mbr[i].first, sizeof(int));
        data += sizeof(int);
    }
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        memcpy(data, &node->bounds->mbr[i].second, sizeof(int));
        data += sizeof(int);
    }
    for (int i = 0; i < node->childIndex.size(); i++)
    {
        memcpy(data, &node->childIndex[i], sizeof(int));
        data += sizeof(int);
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &node->child_bounds[i]->mbr[j].first, sizeof(int));
            data += sizeof(int);
        }
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &node->child_bounds[i]->mbr[j].second, sizeof(int));
            data += 4;
        }
    }
}

Node *getNode(char *data, int nodeID)
{
    while (*((int *)(data)) != nodeID)
        data += NodeSize;

    return extract_node(data);
}

void assignParents(int start, int end, FileManager &fm, FileHandler &fh)
{
    // N = Number of nodes bytes to be inserted into the tree
    int N = end - start;
    int numNodes = 0;
    int startPageNumber, endPageNumber;

    try
    {
        // startPage tells the page to look at for data
        int startPage = start / (PAGE_CONTENT_SIZE / NodeSize);

        PageHandler startPageHandler = fh.PageAt(startPage);
        PageHandler endPageHandler = fh.LastPage();

        // pointers to current data read location and last data read location
        char *startData, *endData;

        startData = startPageHandler.GetData();
        endData = endPageHandler.GetData();
        endData += endPageOffset;

        startPageNumber = startPageHandler.GetPageNum();
        endPageNumber = endPageHandler.GetPageNum();

        fh.MarkDirty(startPageNumber);
        fh.MarkDirty(endPageNumber);

        int offsetData = 0;
        while (*((int *)(startData)) != start)
        {
            startData += NodeSize;
            offsetData += NodeSize;
        }

        while (N > 0)
        {
            Node *newNode = new Node();
            newNode->index = focusIndex;
            focusIndex += 1;

            int nodeCount = 0;

            MBR *newBounds = new MBR(d);
            for (int i = 0; i < d; i++)
            {
                newBounds->mbr[i].first = INT_MAX;
                newBounds->mbr[i].second = INT_MAX;
            }

            while (N > 0 && maxCap > nodeCount)
            {
                if (PAGE_CONTENT_SIZE - NodeSize < offsetData)
                {
                    fh.UnpinPage(startPageNumber);

                    startPageHandler = fh.NextPage(startPageNumber);
                    startPageNumber = startPageHandler.GetPageNum();
                    fh.MarkDirty(startPageNumber);

                    startData = startPageHandler.GetData();
                    offsetData = 0;
                }

                Node *copyNode = extract_node(startData);
                memcpy((startData + sizeof(int)), &newNode->index, sizeof(int));

                copyNode->parentIndex = newNode->index;

                newNode->childIndex[nodeCount] = copyNode->index;

                startData = NodeSize + startData;
                offsetData = NodeSize + offsetData;

                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = copyNode->bounds->mbr[i].first;
                    newNode->child_bounds[nodeCount]->mbr[i].second = copyNode->bounds->mbr[i].second;

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = min(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }
                N--;
                nodeCount++;
                deleteNode(copyNode);
            }

            // newNode->setLeftBounds(newBounds);
            // newNode->setRightBounds(newBounds);
            newNode->setBounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                fh.UnpinPage(endPageNumber);
                endPageHandler = fh.NewPage();
                endPageNumber = endPageHandler.GetPageNum();
                fh.MarkDirty(endPageNumber);

                endData = endPageHandler.GetData();
                endPageOffset = 0;
            }

            copy_data(newNode, endData);
            endData += NodeSize;
            endPageOffset += NodeSize;
            numNodes++;

            deleteNode(newNode);
        }
    }
    catch (InvalidPageException)
    {
        cout << "[*] ERROR: Page is Invalid" << endl;
    }

    fh.UnpinPage(startPageNumber);
    fh.UnpinPage(endPageNumber);
    fh.FlushPages();

    if (numNodes == 1)
    {
        rootIndex = focusIndex - 1;
    }
    else if (numNodes != 1)
    {
        assignParents(end, end + numNodes, fm, fh);
    }

    return;
}

void bulkLoad(int n, FileManager &fm, FileHandler &startFileHandler, FileHandler &endFileHandler)
{
    int startPageNumber, endPageNumber;
    int N = n;

    int numNodes = 0;
    try
    {
        char *startData, *endData;

        PageHandler startPageHandler = startFileHandler.FirstPage();
        PageHandler endPageHandler = endFileHandler.NewPage();

        startPageNumber = startPageHandler.GetPageNum();
        endPageNumber = endPageHandler.GetPageNum();

        startData = startPageHandler.GetData();
        endFileHandler.MarkDirty(endPageNumber);
        endData = endPageHandler.GetData();

        int pageOffset = 0;
        while (N > 0)
        {
            Node *newNode = new Node();
            newNode->index = focusIndex;
            focusIndex += 1;

            int nodeCount = 0;

            MBR *newBounds = new MBR(d);

            while (N > 0 && maxCap > nodeCount)
            {
                if (PAGE_CONTENT_SIZE - (d * sizeof(int)) < pageOffset)
                {
                    startFileHandler.UnpinPage(startPageNumber);
                    startPageHandler = startFileHandler.NextPage(startPageNumber);

                    startData = startPageHandler.GetData();
                    startPageNumber = startPageHandler.GetPageNum();

                    pageOffset = 0;
                }

                newNode->childIndex[nodeCount] = -1;
                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = *(int *)(startData);
                    newNode->child_bounds[nodeCount]->mbr[i].second = INT_MIN;
                    startData += sizeof(int);
                    pageOffset += sizeof(int);

                    // newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    // newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }

                for (int i = 0; i < d; i++)
                {
                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }

                nodeCount++;
                N--;
            }

            newNode->setBounds(newBounds);
            // newNode->setLeftBounds(newBounds);
            // newNode->setRightBounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                endFileHandler.UnpinPage(endPageNumber);

                endPageHandler = endFileHandler.NewPage();
                endPageNumber = endPageHandler.GetPageNum();

                endFileHandler.MarkDirty(endPageNumber);

                endData = endPageHandler.GetData();
                endPageOffset = 0;
            }

            copy_data(newNode, endData);
            endData += NodeSize;

            endPageOffset += NodeSize;
            numNodes++;

            deleteNode(newNode);
        }
    }
    catch (InvalidPageException)
    {
        cout << "[*] Error: Page is Invalid." << endl;
    }

    startFileHandler.UnpinPage(startPageNumber);
    endFileHandler.UnpinPage(endPageNumber);

    startFileHandler.FlushPages();
    endFileHandler.FlushPages();

    assignParents(0, numNodes, fm, endFileHandler);
}

// void printFile()
// merge_mbr
MBR *new_area_mbr(MBR *a, MBR *b)
{
    MBR *new_mbr = new MBR();
    for (int i = 0; i < a->mbr.size(); i++)
    {
        new_mbr->mbr[i].first = min(a->mbr[i].first, b->mbr[i].first);
        new_mbr->mbr[i].second = max(a->mbr[i].second, b->mbr[i].second);
    }
    return new_mbr;
}

// convert a single point to mbr
MBR *point_mbr(vector<int> v)
{
    MBR *m = new MBR();
    for (int i = 0; i < v.size(); i++)
    {
        m->mbr[i].first = v[i];
        m->mbr[i].second = v[i];
    }
    return m;
}

MBR *new_area_point(MBR *a, vector<int> p)
{
    MBR *b = point_mbr(p);
    MBR *new_mbr = new_area_mbr(a, b);
    deleteMBR(b);
    return new_mbr;
}
// hyper_area
long int get_area(MBR *m)
{
    long int area = 1;
    for (int i = 0; i < d; i++)
    {
        area *= (m->mbr[i].second - m->mbr[i].first);
    }
    return area;
}
// check if a belongs to the region of b
bool belongs(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr.size(); i++)
    {
        if (a->mbr[i].first < b->mbr[i].first || a->mbr[i].second > b->mbr[i].second)
            return false;
    }
    return true;
}

// check if two mbr's are same
bool is_equal(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr.size(); i++)
    {
        if (!(a->mbr[i].first == b->mbr[i].first && a->mbr[i].second == b->mbr[i].second))
            return false;
    }
    return true;
}

bool search(vector<int> &P, FileHandler &fh, int index)
{
    int page_id = index / (PAGE_CONTENT_SIZE / NodeSize);
    PageHandler ph = fh.PageAt(page_id);
    char *data = ph.GetData();
    Node *n = getNode(data, index);

    if (n->isLeaf())
    {
        for (int i = 0; i < maxCap; i++)
        {
            MBR *m = point_mbr(P);
            if (is_equal(m, n->child_bounds[i]))
            {
                deleteMBR(m);
                deleteNode(n);
                fh.UnpinPage(ph.GetPageNum());
                return true;
            }
            deleteMBR(m);
        }
        deleteNode(n);
        fh.UnpinPage(ph.GetPageNum());
        return false;
    }
    else
    {
        for (int i = 0; i < maxCap; i++)
        {
            if (n->childIndex[i] == INT_MIN)
                break;

            MBR *m = point_mbr(P);
            if (belongs(m, n->bounds))
            {
                if (search(P, fh, n->childIndex[i]))
                {
                    deleteMBR(m);
                    deleteNode(n);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            deleteMBR(m);
        }
    }
    deleteNode(n);
    fh.UnpinPage(ph.GetPageNum());
    return false;
}

void updateMBR(FileHandler &fileHandler, int nodeParentIndex, MBR *newBounds, int nodeIndex, bool split)
{
    if (nodeParentIndex != -1)
    {
        PageHandler pageHandler = fileHandler.PageAt(nodeParentIndex / (PAGE_CONTENT_SIZE / NodeSize));
        fileHandler.MarkDirty(pageHandler.GetPageNum());
        char *data = pageHandler.GetData();

        while (*(int *)(data) != nodeParentIndex)
        {
            data += NodeSize;
        }

        Node *parentNode = getNode(data, nodeParentIndex);

        MBR *parentBounds = new_area_mbr(parentNode->bounds, newBounds);
        data += 8;

        for (int i = 0; i < d; i++)
        {
            memcpy(data, &parentBounds->mbr[i].first, sizeof(int));
            data += sizeof(int);
        }
        for (int i = 0; i < d; i++)
        {
            memcpy(data, &parentBounds->mbr[i].second, sizeof(int));
            data += sizeof(int);
        }

        if (split == true)
        {
            while (*(int *)(data) != nodeIndex)
            {
                data += (4 + 8 * d);
            }
            data += 4;

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &newBounds[j], 4);
                data += 4;
            }
        }
        else
        {
            while (*(int *)(data) != INT_MIN)
            {
                data += (4 + 8 * d);
            }
            memcpy(data, &nodeIndex, 4);
            data += 4;

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &newBounds[j], 4);
                data += 4;
            }
        }

        fileHandler.UnpinPage(pageHandler.GetPageNum());
        fileHandler.FlushPage(pageHandler.GetPageNum());

        updateMBR(fileHandler, nodeParentIndex, parentBounds, parentNode->index, true);
    }
    else
    {
        return;
    }
}

Node *splitInternal(FileHandler &fileHandler, char *data, int NodeIndex, Node *newNode)
{
    return newNode;
}

void update(FileHandler &fileHandler, int nodeParentIndex, Node *node)
{
    PageHandler pageHandler = fileHandler.PageAt(nodeParentIndex / (PAGE_CONTENT_SIZE / NodeSize));
    fileHandler.MarkDirty(pageHandler.GetPageNum());
    char *data = pageHandler.GetData();

    Node *nodeParent = getNode(data, nodeParentIndex);
    MBR *newBounds = node->bounds;

    if (nodeParent->childIndex[maxCap - 1] == INT_MIN)
    {
        updateMBR(fileHandler, nodeParentIndex, newBounds, node->index, false);
        return;
    }
    else
    {
        Node *newNode = splitInternal(fileHandler, data, nodeParentIndex, node);
        update(fileHandler, newNode->index, newNode);

        deleteNode(newNode);
    }

    deleteNode(nodeParent);
    fileHandler.UnpinPage(pageHandler.GetPageNum());
}

int main(int argc, char *argv[])
{
    FileManager fm;
    FileHandler startFileHandler, endFileHandler;
    string input;

    ifstream infile(argv[1]);
    d = stoi(argv[2]);
    maxCap = stoi(argv[3]);

    try
    {
        endFileHandler = fm.CreateFile("answer.txt");
    }
    catch (InvalidPageException)
    {
        fm.DestroyFile("answer.txt");
        endFileHandler = fm.CreateFile("answer.txt");
    }

    cout << "HERE\n";

    ofstream answerFile(argv[4]);

    // NOTE : sizeof(pair<int,int>) = sizeof(int)*2
    NodeSize = (1 + 1 + 2 * d + maxCap + 2 * d * maxCap) * sizeof(int);

    while (getline(infile, input))
    {
        int position = input.find(" ");
        string command = input.substr(0, position);
        input.erase(0, position + 1);

        if (command == "BULKLOAD")
        {
            position = input.find(" ");
            string inputFile = input.substr(0, position);
            startFileHandler = fm.OpenFile(inputFile.c_str());
            input.erase(0, 1 + position);

            position = input.find(" ");
            int N = stoi(input.substr(0, position));
            input.erase(0, 1 + position);

            bulkLoad(N, fm, startFileHandler, endFileHandler);

            answerFile << "BULKLOAD\n\n";

            fm.CloseFile(startFileHandler);
        }
        else if (command == "QUERY")
        {
            vector<int> searchPoint;
            for (int i = 0; i < d; i++)
            {
                position = input.find(" ");
                searchPoint.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }
            if (search(searchPoint, endFileHandler, rootIndex))
            {
                answerFile << "TRUE" << endl;
            }
            else
            {
                answerFile << "FALSE" << endl;
            }

            endFileHandler.FlushPages();
            answerFile << endl;
        }
    }

    fm.CloseFile(endFileHandler);
    fm.DestroyFile("answer.txt");

    infile.close();
    answerFile.close();

    return (0);
}
