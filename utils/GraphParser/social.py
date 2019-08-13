from .base import BaseParser
import os

page_url = "https://snap.stanford.edu/data/ego-Facebook.html"

class FacebookCircleParser(BaseParser):
    def __init__(self, config):
        download_url = "https://snap.stanford.edu/data/facebook_combined.txt.gz"
        name = "facebook_combined.txt"
        weighted = False
        super().__init__(config, name, download_url, weighted)


    def get(self):
        input_graph_dir = self._get_if_necessary()
        if input_graph_dir is None:
            return
        with open(input_graph_dir, 'r') as f:
            lines = f.readlines()
            edges = [line[:-1].split(" ") for line in lines]
            edges = [(int(fro), int(to)) for (fro, to) in edges]
            n_nodes = max([max(edge) for edge in edges]) + 1 # max id is n_nodes - 1
            self.nodes = [[] for _ in range(n_nodes)]

            for fro, to in edges:
                # graph is undirected, so it goes both ways
                self.nodes[fro].append(to)
                self.nodes[to].append(fro)
        self.write() # call the default write

page_url = "https://snap.stanford.edu/data/ego-Twitter.html"

class TwitterCircleParser(BaseParser):
    def __init__(self, config):
        download_url = "https://snap.stanford.edu/data/twitter_combined.txt.gz"
        name = "twitter_combined.txt"
        weighted = False
        super().__init__(config, name, download_url, weighted)


    def get(self):
        input_graph_dir = self._get_if_necessary()
        if input_graph_dir is None:
            return
        with open(input_graph_dir, 'r') as f:
            lines = f.readlines()
            edges = []

            ids = set()
            for line in lines:
                fro, to = line[:-1].split(" ")
                fro, to = (int(fro), int(to))
                edges.append((fro, to))
                ids.update([fro, to])

            id_map = {}
            for i, id in enumerate(ids):
                id_map[id] = i 
            n_nodes = len(ids)
            self.nodes = [[] for _ in range(n_nodes)]

            for fro, to in edges:
                # graph is directed
                self.nodes[id_map[fro]].append(id_map[to])
        self.write() # call the default write

page_url = "https://snap.stanford.edu/data/ego-Gplus.html"

class GoogleCircleParser(BaseParser):
    def __init__(self, config):
        download_url = "https://snap.stanford.edu/data/gplus_combined.txt.gz"
        name = "gplus_combined.txt"
        weighted = False
        super().__init__(config, name, download_url, weighted)

    def get(self):
        input_graph_dir = self._get_if_necessary()
        if input_graph_dir is None:
            return

        outpath = self.graph_path / self.name
        print("Writing the parsed graph {}...".format(outpath))

        n_edges = 0
        n_nodes = 0
        from collections import defaultdict
        edge_count = defaultdict(lambda : 0)
        
        id_map = {}
        ids = set()
        with open(input_graph_dir, 'r') as f:
            l = f.readline()
            while l != "":
                fro, to = l[:-1].split(" ")
                fro, to = int(fro), int(to)
                ids.update([fro, to])
                n_edges += 1
                
                l = f.readline()

        for i, id in enumerate(ids):
            id_map[id] = i 
        n_nodes = len(ids)

        with open(input_graph_dir, 'r') as f:
            l = f.readline()
            while l != "":
                fro, to = l[:-1].split(" ")
                fro, to = id_map[int(fro)], id_map[int(to)]
                edge_count[fro] += 1
                l = f.readline()
        # first output the n_nodes, n_edges, and offsets
        with open(outpath, "w") as f:
            f.write("{}\n".format(n_nodes))
            f.write("{}\n".format(n_edges))

            current_edge = 0
            for i in range(n_nodes):
                f.write("{}\n".format(current_edge))
                current_edge += edge_count[i]

        print("Offsets done. Outputting edges now")

        offset = 500000 # process that many each time
        for i in range(n_nodes // offset + 1):
            min_id = i * offset
            max_id = min((i+1) * offset, n_nodes) # exclusive
            edges = [list() for _ in range(max_id-min_id)]
            with open(input_graph_dir, 'r') as f:
                l = f.readline()
                while l != "":
                    fro, to = l[:-1].split(" ")
                    fro, to = id_map[int(fro)], id_map[int(to)]
                   
                    if (min_id <= fro < max_id):
                        edges[fro-min_id].append(to)
                
                    l = f.readline()

            with open(outpath, "a") as f:
                for edge in edges:
                    for neighbor in edge:
                        f.write("{}\n".format(neighbor))

            print("{0:.0%} done".format(i / (n_nodes // offset + 1)))

        

__all__ = ["FacebookCircleParser", "TwitterCircleParser", "GoogleCircleParser"]