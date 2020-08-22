#include "constants.h"
#include "buffer_manager.h"
#include "errors.h"
#include "file_manager.h"

#include <iostream>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>
#include <fstream>

using std::cin;
using std::cout;
using std::ifstream;
using std::string;
using std::vector;

namespace RTree
{
    static int d = 2;
    static int maxCap = 10;
    static int NODE_SIZE = 0;
    static int gid = 0;
    static int root_id = -1;

    int last_page_offset = 0;

    class Node
    {
    public:
        int id;
        int pid;

        vector<int> MBR;
        vector<int> child_id;
        vector<vector<int>> child_MBR;

        explicit Node()
        {
            // cout << "Created node with id: " << gid << endl;
            this->pid = -1;
            this->id = -1;

            this->MBR.resize(2 * d);
            this->child_id.resize(maxCap, INT_MIN);
            this->child_MBR.resize(maxCap);
            for (int i = 0; i < maxCap; i++)
                this->child_MBR[i].resize(2 * d, INT_MIN);
        }

        void set_mbr(const vector<int> &mbr)
        {
            this->MBR = mbr;
        }

        void set_mbr_bl(vector<int> &pt)
        {
            for (int i = 0; i < d; i++)
                this->MBR[i] = pt[i];
        }

        void set_mbr_tr(vector<int> &pt)
        {
            for (int i = 0; i < d; i++)
                this->MBR[d + i] = pt[i];
        }

        void print_node()
        {
            cout << "====================================" << endl;
            cout << "id: " << this->id << "       pid: " << this->pid << endl;
            cout << "MBR: " << endl;
            for (int i = 0; i < d; i++)
                cout << this->MBR[i] << "   ";
            cout << endl;
            for (int i = 0; i < d; i++)
                cout << this->MBR[d + i] << "   ";
            cout << endl;
            cout << "Children: ";
            for (int i = 0; i < maxCap; i++)
                cout << this->child_id[i] << "  ";
            cout << endl;
            cout << "====================================" << endl;
        }

        bool is_leaf()
        {
            if (this->child_id[0] == -1)
                return true;
            else
                return false;
        }
    };

    void delete_node(Node *n)
    {
        n->MBR.clear();
        n->MBR.shrink_to_fit();
        n->child_id.clear();
        n->child_id.shrink_to_fit();
        n->child_MBR.clear();
        n->child_MBR.shrink_to_fit();
        delete(n);
    }

    Node *extract_node(char *data)
    {
        Node *n = new Node();
        n->id = *(int *)(data);
        data += sizeof(int);

        n->pid = *(int *)(data);
        data += sizeof(int);

        for (int i = 0; i < 2 * d; i++)
        {
            n->MBR[i] = *(int *)(data);
            data += sizeof(int);
        }

        for (int i = 0; i < maxCap; i++)
        {
            n->child_id[i] = *(int *)(data);
            data += sizeof(int);

            for (int j = 0; j < 2 * d; j++)
            {
                n->child_MBR[i][j] = *(int *)(data);
                data += sizeof(int);
            }
        }

        return n;
    }

    void copy_data(char *wfdata, Node *n)
    {
        memcpy(wfdata, &n->id, sizeof(int));
        wfdata += sizeof(int);

        memcpy(wfdata, &n->pid, sizeof(int));
        wfdata += sizeof(int);

        for (int i = 0; i < 2 * d; i++)
        {
            memcpy(wfdata, &n->MBR[i], sizeof(int));
            wfdata += sizeof(int);
        }

        for (int i = 0; i < maxCap; i++)
        {
            memcpy(wfdata, &n->child_id[i], sizeof(int));
            wfdata += sizeof(int);

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(wfdata, &n->child_MBR[i][j], sizeof(int));
                wfdata += sizeof(int);
            }
        }

        return;
    }

    // pass through the data array and gets complete node vector
    Node *get_node(char *data, int node_id)
    {
        while (*((int *)(data)) != node_id)
            data += NODE_SIZE;

        return extract_node(data);
    }

    void assign_parents(FileManager &fm, FileHandler &fh, int start, int end)
    {
        int N = end - start;
        int num_nodes = 0;
        int page_no, wpage_no;

        try
        {
            int k = start / (PAGE_CONTENT_SIZE / NODE_SIZE);
            // cout << "Start page offset: " << k << endl;

            PageHandler ph = fh.PageAt(k);

            char *data, *wdata;

            data = ph.GetData();
            page_no = ph.GetPageNum();
            fh.MarkDirty(page_no);

            PageHandler wph = fh.LastPage();

            wpage_no = wph.GetPageNum();
            fh.MarkDirty(wpage_no);

            wdata = wph.GetData();
            wdata += last_page_offset;

            // cout << "Last page offset: " << last_page_offset << endl;

            int offset = 0;
            while (*((int *)(data)) != start)
            {
                // cout << "id: " << *((int *)(data)) << endl;
                data += NODE_SIZE;
                offset += NODE_SIZE;
            }

            // cout << "Start offset: " << offset << endl;

            while (N > 0)
            {
                int count = 0;
                Node *n = new Node();
                n->id = gid++;

                vector<int> pt_bl(d, INT_MAX);
                vector<int> pt_tr(d, INT_MIN);

                while (count < maxCap && N > 0)
                {
                    if (offset > PAGE_CONTENT_SIZE - NODE_SIZE)
                    {
                        // cout << "Switching page" << endl;
                        fh.UnpinPage(page_no);

                        ph = fh.NextPage(page_no);
                        page_no = ph.GetPageNum();
                        fh.MarkDirty(page_no);
                        data = ph.GetData();
                        offset = 0;
                    }

                    // Node *cn = (Node *)(data);
                    Node *cn = extract_node(data);
                    // cout << cn->id << ": " << *(int*)(data+4) << endl;
                    memcpy((data + 4), &n->id, sizeof(int));
                    // cout << cn->id << ": " << *(int*)(data+4) << endl;

                    // cn->print_node();
                    data += NODE_SIZE;
                    offset += NODE_SIZE;

                    cn->pid = n->id;

                    n->child_id[count] = cn->id;

                    for (int j = 0; j < d; j++)
                    {
                        n->child_MBR[count][j] = cn->MBR[j];
                        n->child_MBR[count][d + j] = cn->MBR[d + j];

                        pt_bl[j] = min(pt_bl[j], n->child_MBR[count][j]);
                        pt_tr[j] = max(pt_tr[j], n->child_MBR[count][d + j]);
                    }

                    count++;
                    N--;

                    delete_node(cn);
                }

                n->set_mbr_bl(pt_bl);
                n->set_mbr_tr(pt_tr);

                if (last_page_offset > PAGE_CONTENT_SIZE - NODE_SIZE)
                {
                    fh.UnpinPage(wpage_no);

                    wph = fh.NewPage();
                    wpage_no = wph.GetPageNum();
                    fh.MarkDirty(wpage_no);

                    wdata = wph.GetData();
                    last_page_offset = 0;
                }

                // memcpy(wdata, n, NODE_SIZE);
                copy_data(wdata, n);
                // n->print_node();
                wdata += NODE_SIZE;
                last_page_offset += NODE_SIZE;
                num_nodes++;

                delete_node(n);

                // cout << "N: " << N << " num_nodes: " << num_nodes << endl;
            }
        }
        catch (InvalidPageException)
        {
            cout << "Invalid Page Exception" << endl;
        }

        fh.UnpinPage(page_no);
        fh.UnpinPage(wpage_no);
        fh.FlushPages();

        if (num_nodes == 1)
        {
            root_id = gid - 1;
            // cout << "Assign Parents completed!" << endl;
        }
        else if (num_nodes != 1)
            assign_parents(fm, fh, end, end + num_nodes);

        return;
    }

    void bulk_load(FileManager &fm, FileHandler &fh, FileHandler &wfh, int n)
    {
        int page_no, wfpage_no;
        int N = n;

        int num_nodes = 0;
        try
        {
            char *data, *wfdata;

            PageHandler ph = fh.FirstPage();
            page_no = ph.GetPageNum();
            data = ph.GetData();

            PageHandler wfph = wfh.NewPage();
            wfpage_no = wfph.GetPageNum();
            wfh.MarkDirty(wfpage_no);
            wfdata = wfph.GetData();

            int offset = 0;
            while (N > 0)
            {
                int count = 0;
                Node *n = new Node();
                n->id = gid++;

                vector<int> pt_bl(d, INT_MAX);
                vector<int> pt_tr(d, INT_MIN);

                while (count < maxCap && N > 0)
                {
                    if (offset > PAGE_CONTENT_SIZE - (d * sizeof(int)))
                    {
                        fh.UnpinPage(page_no);
                        ph = fh.NextPage(page_no);

                        data = ph.GetData();
                        page_no = ph.GetPageNum();

                        offset = 0;
                    }

                    n->child_id[count] = -1;
                    for (int j = 0; j < d; j++)
                    {
                        n->child_MBR[count][j] = *(int *)(data);
                        n->child_MBR[count][d + j] = INT_MIN;
                        data += sizeof(int);
                        offset += sizeof(int);

                        pt_bl[j] = min(pt_bl[j], n->child_MBR[count][j]);
                        pt_tr[j] = max(pt_tr[j], n->child_MBR[count][j]);
                    }

                    count++;
                    N--;
                }

                n->set_mbr_bl(pt_bl);
                n->set_mbr_tr(pt_tr);

                if (last_page_offset > PAGE_CONTENT_SIZE - NODE_SIZE)
                {
                    wfh.UnpinPage(wfpage_no);

                    wfph = wfh.NewPage();
                    wfpage_no = wfph.GetPageNum();

                    wfh.MarkDirty(wfpage_no);

                    wfdata = wfph.GetData();
                    last_page_offset = 0;
                }

                // memcpy(wfdata, n, NODE_SIZE);
                copy_data(wfdata, n);

                wfdata += NODE_SIZE;
                // n->print_node();
                last_page_offset += NODE_SIZE;
                num_nodes++;

                delete (n);
            }
        }
        catch (InvalidPageException)
        {
            cout << "Invalid Page Exception!" << endl;
        }

        // fm.PrintBuffer();

        fh.UnpinPage(page_no);
        wfh.UnpinPage(wfpage_no);

        // fm.PrintBuffer();

        fh.FlushPages();
        wfh.FlushPages();

        assign_parents(fm, wfh, 0, num_nodes);
    }

    void print_file(const char *filename, FileManager &fm)
    {
        cout << "Printing file contents: " << filename << endl;
        FileHandler fh = fm.OpenFile(filename);

        try
        {
            char *data;
            int page_no;

            PageHandler ph = fh.FirstPage();
            page_no = ph.GetPageNum();
            data = ph.GetData();

            int offset = 0;

            while (true)
            {
                if (offset > PAGE_CONTENT_SIZE - NODE_SIZE)
                {
                    fh.UnpinPage(page_no);
                    ph = fh.NextPage(page_no);
                    page_no = ph.GetPageNum();
                    data = ph.GetData();
                    offset = 0;
                }

                // Node *n = (Node *)(data);
                Node *n = extract_node(data);
                data += NODE_SIZE;
                offset += NODE_SIZE;

                n->print_node();
            }
        }
        catch (InvalidPageException)
        {
        }
    }

    int get_page(int node_id)
    {
        return node_id / (PAGE_CONTENT_SIZE / NODE_SIZE);
    }

    bool is_equal(vector<int> &a, vector<int> &b, int d)
    {
        for (int i = 0; i < d; i++)
        {
            if (a[i] != b[i])
                return false;
        }

        return true;
    }

    // check if a point Q lies inside a given mbr
    bool belongs(vector<int> Q, vector<int> mbr, int d)
    {
        for (int i = 0; i < d; i = i + 1)
        {
            if (!(Q[i] >= mbr[i] && Q[i] <= mbr[i + d]))
                return false;
        }

        return true;
    }

    bool search(vector<int> &P, FileHandler &fh, int node_id)
    {
        // cout << get_page(node_id) << endl;
        PageHandler ph = fh.PageAt(get_page(node_id));

        char *data = ph.GetData();
        Node *node = get_node(data, node_id);
        // node->print_node();

        if (node->is_leaf())
        {
            // cout << node->id << " is a leaf" << endl;
            for (int i = 0; i < maxCap; i++)
            {
                if (is_equal(P, node->child_MBR[i], d))
                {
                    delete_node(node);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            delete_node(node);
            fh.UnpinPage(ph.GetPageNum());
            return false;
        }

        for (int i = 0; i < maxCap; i++)
        {
            // cout << node->id << " is not a leaf" << endl;
            if (node->child_id[i] == INT_MIN)
                break;

            if (belongs(P, node->child_MBR[i], d))
            {
                // std::cerr << "Search 3.1 " << "\n";
                if (search(P, fh, node->child_id[i]))
                {
                    delete_node(node);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            // std::cerr << "Search 3.2\n";            
        }

        delete_node(node);
        fh.UnpinPage(ph.GetPageNum());
        return false;
    }

    // given a new mbr gives updates the mbr so as to cover new mbr
    vector<int> new_area_mbr(vector<int> &mbr, vector<int> &P)
    {
        vector<int> mbr1_s;
        vector<int> mbr1_b;
        for (int i = 0; i < d; i++)
        {
            mbr1_s.push_back(min(mbr[i], P[i]));
            mbr1_b.push_back(max(mbr[i + d], P[i + d]));
        }

        mbr1_s.insert(mbr1_s.end(), mbr1_b.begin(), mbr1_b.end());
        return mbr1_s;
    }

    // given a point gives updates the mbr so as to cover point
    vector<int> new_area_point(vector<int> mbr, vector<int> Q)
    {
        vector<int> mbr1_s;
        vector<int> mbr1_b;

        for (int i = 0; i < d; i++)
        {
            mbr1_s.push_back(min(mbr[i], Q[i]));
            mbr1_b.push_back(max(mbr[i + d], Q[i]));
        }

        mbr1_s.insert(mbr1_s.end(), mbr1_b.begin(), mbr1_b.end());
        return mbr1_s;
    }

    // Get area given an mbr
    long long int get_area(vector<int> mbr)
    {
        long long int area = 1;
        for (int i = 0; i < d; i = i + 1)
        {
            area = area * (mbr[i + d] - mbr[i]);
        }

        return area;
    }

    // Starts from the root and returns the leaf to which a new point belongs
    int find_leaf(vector<int> &P, FileHandler &fh, int node_id)
    {
        PageHandler ph = fh.PageAt(get_page(node_id));
        char *data = ph.GetData();

        // vector<int> node_info = get_node_info(data, node_id);
        Node *node = get_node(data, node_id);
        // leaf case
        if (node->child_id[0] == -1)
            return node_id;

        int min_inc = INT_MAX;
        int child = -1;
        for (int i = 0; i < maxCap; i++)
        {
            if (node->child_id[i] == INT_MIN)
            {
                continue;
            }
            else
            {
                long long int area1 = get_area(node->child_MBR[i]);
                long long int inc = get_area(new_area_point(node->child_MBR[i], P)) - area1;
                if (inc < min_inc)
                {
                    min_inc = inc;
                    child = i;
                }

                else if (inc == min_inc)
                {
                    long long int area2 = get_area(node->child_MBR[child]);

                    if (area2 > area1)
                        child = i;
                }
            }
        }

        fh.UnpinPage(ph.GetPageNum());
        // fh.FlushPage(ph.GetPageNum());
        return find_leaf(P, fh, node->child_id[child]);
    }

    // gets distance between e1, e2
    long long int find_distance(vector<int> &P1, vector<int> &P2)
    {
        long long int distance = 0;
        for (int i = 0; i < P1.size(); i++)
        {
            distance += (P1[i] - P2[i]) * (P1[i] - P2[i]);
        }
        return distance;
    }

    // updates a splitted node in the data array
    void update_node(char *data, Node *n)
    {
        while (*(int *)(data) != n->id)
            data += NODE_SIZE;

        copy_data(data, n);
    }

    //updates mbr of whole tree mode = 1 -> in case of no split 2-> in case of split
    void update_mbr(int parent_id, vector<int> &new_mbr, FileHandler &fh, int child_id, int mode)
    {
        if (parent_id == -1)
            return;

        PageHandler ph = fh.PageAt(get_page(parent_id));
        fh.MarkDirty(ph.GetPageNum());
        char *data = ph.GetData();

        while (*(int *)(data) != parent_id)
            data += NODE_SIZE;

        Node *parent = get_node(data, parent_id);

        vector<int> parent_new_mbr = new_area_mbr(parent->MBR, new_mbr);

        data += 2 * sizeof(int);
        for (int j = 0; j < 2 * d; j = j + 1)
        {
            memcpy(data, &parent_new_mbr[j], sizeof(int));
            data += sizeof(int);
        }

        if (mode == 1)
        {
            while (*(int *)(data) != child_id)
                data += (4 + 8 * d);

            data += sizeof(int);

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &new_mbr[j], sizeof(int));
                data += sizeof(int);
            }
        }
        else
        {
            while (*(int *)(data) != INT_MIN)
                data += (4 + 8 * d);

            memcpy(data, &child_id, sizeof(int));
            data += sizeof(int);

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &new_mbr[j], sizeof(int));
                data += sizeof(int);
            }
        }

        fh.UnpinPage(ph.GetPageNum());
        fh.FlushPage(ph.GetPageNum());

        update_mbr(parent->pid, parent_new_mbr, fh, parent_id, 1);
    }

    // add a new splitted node at the end
    void add_to_page(Node *n, FileHandler &fh)
    {
        PageHandler ph = fh.LastPage();

        int x = get_page(n->id);
        int y = ph.GetPageNum();

        if (x == y + 1)
        {
            fh.UnpinPage(y);
            fh.FlushPage(y);
            ph = fh.NewPage();
            last_page_offset = 0;

            char *data = ph.GetData();

            copy_data(data, n);
        }
        else
        {
            fh.MarkDirty(ph.GetPageNum());
            char *data = ph.GetData();
            while (*(int *)(data) != n->id - 1)
                data += NODE_SIZE;

            data += NODE_SIZE;

            copy_data(data, n);
        }

        fh.UnpinPage(ph.GetPageNum());
        // fh.FlushPage(ph.GetPageNum());
    }

    //splits leaf node
    Node *split(int node_id, char *data, FileHandler &fh, vector<int> P)
    {
        Node *node = get_node(data, node_id);
        vector<vector<int>> child_mbr = node->child_MBR;
        int old_id = node->id;

        P.resize(2 * d, INT_MIN);

        child_mbr.push_back(P);

        int parent_id = node->pid;

        int e1 = -1;
        int e2 = -1;

        vector<int> E1;
        vector<int> E2;

        vector<int> mbr1;
        vector<int> mbr2;

        long long int max_dist = INT_MIN;

        for (int i = 0; i < child_mbr.size(); i++)
        {
            for (int j = i + 1; j < child_mbr.size(); j++)
            {
                long long int dist = find_distance(child_mbr[i], child_mbr[j]);
                if (dist > max_dist)
                {
                    max_dist = dist;
                    e1 = i;
                    e2 = j;
                }
            }
        }

        for (int i = 0; i < d; i++)
        {
            mbr1.push_back(child_mbr[e1][i]);
            mbr2.push_back(child_mbr[e2][i]);
        }

        for (int i = 0; i < d; i++)
        {
            mbr1.push_back(mbr1[i]);
            mbr2.push_back(mbr2[i]);
        }

        int count = 0;
        for (int i = 0; i < child_mbr.size(); i++)
        {
            long long int area1_i = get_area(mbr1);
            vector<int> a1 = new_area_point(mbr1, child_mbr[i]);
            long long int area1 = get_area(a1) - area1_i;

            long long int area2_i = get_area(mbr2);
            vector<int> a2 = new_area_point(mbr2, child_mbr[i]);
            long long int area2 = get_area(a2) - area2_i;

            count = count + 1;

            if (area1 > area2)
            {
                E2.push_back(i);
                mbr2 = a2;
            }
            else if (area1 < area2)
            {
                E1.push_back(i);
                mbr1 = a1;
            }
            else
            {
                if (area1_i < area2_i)
                {
                    E1.push_back(i);
                    mbr1 = a1;
                }
                else if (area2_i < area1_i)
                {
                    E2.push_back(i);
                    mbr2 = a2;
                }
                else
                {
                    if (E1.size() < E2.size())
                    {
                        E1.push_back(i);
                        mbr1 = a1;
                    }
                    else
                    {
                        E2.push_back(i);
                        mbr2 = a2;
                    }
                }
            }

            if (child_mbr.size() - count == int(ceil((float)maxCap / 2.0)) - E1.size())
            {
                for (int j = i + 1; j < child_mbr.size(); j++)
                {
                    vector<int> a1 = new_area_point(mbr1, child_mbr[j]);
                    E1.push_back(j);
                    mbr1 = a1;
                }
                break;
            }

            if (child_mbr.size() - count == int(ceil((float)maxCap / 2.0)) - E2.size())
            {
                for (int j = i + 1; j < child_mbr.size(); j++)
                {
                    vector<int> a2 = new_area_point(mbr2, child_mbr[j]);
                    E2.push_back(j);
                    mbr2 = a2;
                }
                break;
            }
        }

        // cout << E1.size() << "|" << E2.size() << endl;
        node->MBR = mbr1;

        for (int i = 0; i < maxCap; i++)
        {
            if (i < E1.size())
            {
                node->child_id[i] = -1;
                node->child_MBR[i] = child_mbr[E1[i]];
            }
            else
            {
                node->child_id[i] = INT_MIN;
                node->child_MBR[i].assign(2 * d, INT_MIN);
            }
        }

        // node->print_node();

        Node *new_node = new Node();
        new_node->id = gid++;
        new_node->pid = parent_id;
        new_node->MBR = mbr2;

        for (int i = 0; i < maxCap; i++)
        {
            if (i < E2.size())
            {
                new_node->child_id[i] = -1;
                new_node->child_MBR[i] = child_mbr[E2[i]];
            }
            else
            {
                new_node->child_id[i] = INT_MIN;
                new_node->child_MBR[i].assign(2 * d, INT_MIN);
            }
        }

        if (parent_id == -1)
        {
            Node *root = new Node();
            root->id = gid++;
            root->pid = -1;

            root_id = root->id;

            vector<int> mbr = new_area_mbr(mbr1, mbr2);
            root->MBR = mbr;

            root->child_id[0] = node->id;
            root->child_MBR[0] = node->MBR;

            // root->child_id[1] = newer_node->id;
            // root->child_MBR[1] = newer_node->MBR;

            new_node->pid = root_id;
            node->pid = root_id;

            add_to_page(new_node, fh);
            update_node(data, node);

            add_to_page(root, fh);

            delete_node(root);
            return new_node;
        }

        update_node(data, node);
        update_mbr(node->pid, node->MBR, fh, node->id, 1);
        // new_node->print_node();
        add_to_page(new_node, fh);

        delete_node(node);
        return new_node;
    }

    // updates the parent of children of a new splitted node
    void update_parents(vector<int> &child_id, vector<int> &E, int parent_id, FileHandler &fh)
    {
        for (int i = 0; i < E.size(); i++)
        {
            int id = child_id[E[i]];
            PageHandler ph = fh.PageAt(get_page(id));
            fh.MarkDirty(ph.GetPageNum());

            char *data = ph.GetData();

            while (*(int *)(data) != id)
                data += NODE_SIZE;
            data += sizeof(int);
            memcpy(data, &parent_id, sizeof(int));

            fh.UnpinPage(ph.GetPageNum());
        }
    }

    //split an internal node
    Node *split_internal(int node_id, char *data, FileHandler &fh, Node *new_node)
    {
        Node *node = get_node(data, node_id);
        // node->print_node();

        vector<vector<int>> child_mbr = node->child_MBR;
        vector<int> child_id = node->child_id;
        int old_id = node->id;

        child_mbr.push_back(new_node->MBR);
        child_id.push_back(new_node->id);

        int parent_id = node->pid;

        int e1 = -1;
        int e2 = -1;

        vector<int> E1;
        vector<int> E2;

        vector<int> mbr1;
        vector<int> mbr2;

        long long int max_dist = LONG_MIN;

        for (int i = 0; i < child_mbr.size(); i++)
        {
            for (int j = i + 1; j < child_mbr.size(); j++)
            {
                long long int dist = find_distance(child_mbr[i], child_mbr[j]);
                if (dist > max_dist)
                {
                    max_dist = dist;
                    e1 = i;
                    e2 = j;
                }
            }
        }

        mbr1 = child_mbr[e1];
        mbr2 = child_mbr[e2];

        int count = 0;
        for (int i = 0; i < child_mbr.size(); i++)
        {
            long long int area1_i = get_area(mbr1);
            vector<int> a1 = new_area_mbr(mbr1, child_mbr[i]);
            long long int area1 = get_area(a1) - area1_i;

            long long int area2_i = get_area(mbr2);
            vector<int> a2 = new_area_mbr(mbr2, child_mbr[i]);
            long long int area2 = get_area(a2) - area2_i;
            count = count + 1;

            if (area1 > area2)
            {
                E2.push_back(i);
                mbr2 = a2;
            }

            else if (area2 < area1)
            {
                E1.push_back(i);
                mbr1 = a1;
            }
            else
            {
                if (area1_i < area2_i)
                {
                    E1.push_back(i);
                    mbr1 = a1;
                }
                else if (area2_i < area1_i)
                {
                    E2.push_back(i);
                    mbr2 = a2;
                }
                else
                {
                    if (E1.size() < E2.size())
                    {
                        E1.push_back(i);
                        mbr1 = a1;
                    }
                    else
                    {
                        E2.push_back(i);
                        mbr2 = a2;
                    }
                }
            }

            if (child_mbr.size() - count == int(ceil((float)maxCap / 2.0)) - E1.size())
            {
                for (int j = i + 1; j < child_mbr.size(); j++)
                {
                    vector<int> a1 = new_area_mbr(mbr1, child_mbr[j]);
                    E1.push_back(j);
                    mbr1 = a1;
                }
                break;
            }

            if (child_mbr.size() - count == int(ceil((float)maxCap / 2.0)) - E2.size())
            {
                for (int j = i + 1; j < child_mbr.size(); j++)
                {
                    vector<int> a2 = new_area_mbr(mbr2, child_mbr[j]);
                    E2.push_back(j);
                    mbr2 = a2;
                }
                break;
            }
        }

        node->MBR = mbr1;
        // cout << E1.size() << "|" << E2.size() << endl;

        for (int i = 0; i < maxCap; i++)
        {
            if (i < E1.size())
            {
                node->child_id[i] = child_id[E1[i]];
                node->child_MBR[i] = child_mbr[E1[i]];
            }
            else
            {
                node->child_id[i] = INT_MIN;
                node->child_MBR[i].assign(2 * d, INT_MIN);
            }
        }

        Node *newer_node = new Node();
        newer_node->id = gid++;
        newer_node->pid = parent_id;
        newer_node->MBR = mbr2;

        for (int i = 0; i < maxCap; i++)
        {
            if (i < E2.size())
            {
                newer_node->child_id[i] = child_id[E2[i]];
                newer_node->child_MBR[i] = child_mbr[E2[i]];
            }
            else
            {
                newer_node->child_id[i] = INT_MIN;
                newer_node->child_MBR[i].assign(2 * d, INT_MIN);
            }
        }

        // newer_node->print_node();
        update_parents(child_id, E2, newer_node->id, fh);

        if (parent_id == -1)
        {
            Node *root = new Node();
            root->id = gid++;
            root->pid = -1;

            root_id = root->id;

            vector<int> mbr = new_area_mbr(mbr1, mbr2);
            root->MBR = mbr;

            root->child_id[0] = node->id;
            root->child_MBR[0] = node->MBR;

            // root->child_id[1] = newer_node->id;
            // root->child_MBR[1] = newer_node->MBR;

            newer_node->pid = root_id;
            node->pid = root_id;

            add_to_page(newer_node, fh);
            update_node(data, node);

            add_to_page(root, fh);

            delete_node(root);
            return newer_node;
        }

        update_node(data, node);
        update_mbr(node->pid, node->MBR, fh, node->id, 1);
        // newer_node->print_node();
        add_to_page(newer_node, fh);

        delete_node(node);

        return newer_node;
    }

    
    // add the new formed node to the parent node and updates the whole tree in case of a leaf split
    void update(int parent_id, Node *new_node, FileHandler &fh)
    {
        // cout << parent_id << " || " << get_page(parent_id) << endl;
        PageHandler ph = fh.PageAt(get_page(parent_id));
        fh.MarkDirty(ph.GetPageNum());
        char *data = ph.GetData();
        
        Node *parent = get_node(data, parent_id);
        vector<int> new_mbr = new_node->MBR;

        // parent->print_node();
        if (parent->child_id[maxCap - 1] != INT_MIN)
        {
            Node *newer_node = split_internal(parent_id, data, fh, new_node);
            // newer_node->print_node();
            update(newer_node->pid, newer_node, fh);

            delete_node(newer_node);
        }
        else
        {
            update_mbr(parent_id, new_mbr, fh, new_node->id, 0);
            return;
        }

        delete_node(parent);

        fh.UnpinPage(ph.GetPageNum());
    }

    // in case of no split adds a point to leaf
    void add_point(int leaf_id, char *data, vector<int> &P, vector<int> &mbr)
    {
        int i = 0;
        int num = -1;

        while (*(int *)(data) != leaf_id)
            data += NODE_SIZE;

        data += 2 * sizeof(int);
        for (int j = 0; j < 2 * d; j++)
        {
            num = mbr[j];
            memcpy(data, &num, sizeof(int));
            data += sizeof(int);
        }

        while (*(int *)(data) != INT_MIN)
            data += (4 + 8 * d);

        memcpy(data, &num, sizeof(int));
        data += sizeof(int);

        for (int j = 0; j < d; j++)
        {
            memcpy(data, &P[j], sizeof(int));
            data += sizeof(int);
        }
    }

    // insert a new point to tree
    void insert(vector<int> &P, FileHandler &fh, int node_id, FileManager &fm)
    {
        int leaf_id = find_leaf(P, fh, node_id);
        // cout << leaf_id << endl;
        fh.FlushPages();

        PageHandler ph = fh.PageAt(get_page(leaf_id));
        fh.MarkDirty(ph.GetPageNum());

        char *data = ph.GetData();
        Node *node = get_node(data, leaf_id);

        if (node->child_id[maxCap - 1] != INT_MIN)
        {
            // split the node in two parts and add the new node to the end page
            Node *new_node = split(leaf_id, data, fh, P);
            // new_node->print_node();
            fh.UnpinPage(ph.GetPageNum());
            fh.FlushPage(ph.GetPageNum());

            // cout << "new node id: " << new_node->id << endl;
            // update the child of the parents and if parent is full then again a split
            update(new_node->pid, new_node, fh);

            delete_node(new_node);
        }
        else
        {
            vector<int> new_mbr = new_area_point(node->MBR, P);
            //just add a point and change the mbr of the node
            add_point(leaf_id, data, P, new_mbr);

            fh.UnpinPage(ph.GetPageNum());
            fh.FlushPage(ph.GetPageNum());

            //update all the mbr from leaf upto the top --- 1 indicates update upto the root
            update_mbr(node->pid, new_mbr, fh, leaf_id, 1);
        }

        delete_node(node);

        fh.FlushPages();
    }

    // check if a point Q lies inside a given mbr
    bool belongs(vector<int> &Q, vector<int> &mbr)
    {
        vector<int> P;
        for (int i = 0; i < Q.size(); i++)
        {
            if (Q[i] == INT_MIN)
                continue;
            P.push_back(Q[i]);
        }

        for (int i = 0; i < d; i = i + 1)
        {
            if (P[i] >= mbr[i] && P[i] <= mbr[i + d])
            {
                continue;
            }
            else
                return false;
        }

        return true;
    }

} // namespace RTree

using namespace RTree;

int main(int argc, char *argv[])
{
    FileManager fm;
    FileHandler fh, wfh;
    string line;

    ifstream infile(argv[1]);
    maxCap = stoi(argv[2]);
    d = stoi(argv[3]);

    try {
        wfh = fm.CreateFile("output.txt");
    }
    catch(InvalidFileException)
    {
        fm.DestroyFile("output.txt");
        wfh = fm.CreateFile("output.txt");
    }

    ofstream outfile(argv[4]);

    NODE_SIZE = (1 + 1 + 2 * d + maxCap + 2 * d * maxCap) * sizeof(int);
    // cout << "NODE_SIZE = " << NODE_SIZE << endl;

    while (getline(infile, line))
    {
        // cout << line << endl;
        int pos = line.find(' ');
        string cmd = line.substr(0, pos);
        line.erase(0, pos + 1);

        if (cmd == "BULKLOAD")
        {
            pos = line.find(' ');
            string filename = line.substr(0, pos);
            fh = fm.OpenFile(filename.c_str());
            line.erase(0, pos + 1);

            pos = line.find(' ');
            int N = stoi(line.substr(0, pos));
            line.erase(0, pos + 1);

            bulk_load(fm, fh, wfh, N);
            outfile << "BULKLOAD" << endl << endl;

            fm.CloseFile(fh);
        }
        else if (cmd == "QUERY")
        {
            vector<int> P;
            for (int i = 0; i < d; i++)
            {
                pos = line.find(' ');
                P.push_back(stoi(line.substr(0, pos)));
                line.erase(0, pos + 1);
            }

            if (search(P, wfh, root_id)){
                // std::cerr << "TRUE" << "\n";
                outfile << "TRUE" << endl;
            }
            else{
                // std::cerr << "FALSE" << "\n";
                outfile << "FALSE" << endl;
            }

            wfh.FlushPages();

            outfile << endl;
        }
        else if (cmd == "INSERT")
        {
            vector<int> P;
            for (int i = 0; i < d; i++)
            {
                pos = line.find(' ');
                P.push_back(stoi(line.substr(0, pos)));
                line.erase(0, pos + 1);
            }
            
            if (root_id == -1)
            {
                PageHandler ph = wfh.NewPage();
                char *data = ph.GetData();

                Node *n = new Node();
                n->id = gid++;
                n->pid = -1;

                n->child_id[0] = -1;
                for (int i = 0; i < d; i++)
                {
                    n->MBR[i] = P[i];
                    n->MBR[i + d] = P[i];

                    n->child_MBR[0][i] = P[i];
                }

                root_id = 0;

                copy_data(data, n);
                wfh.UnpinPage(ph.GetPageNum());
                wfh.FlushPage(ph.GetPageNum());
            }
            else
                insert(P, wfh, root_id, fm);
            outfile << "INSERT" << endl << endl; 

            wfh.FlushPages();
        }
    }

    // Close the file
    fm.CloseFile(wfh);
    fm.DestroyFile("output.txt");

    infile.close();
    outfile.close();
    return 0;
}