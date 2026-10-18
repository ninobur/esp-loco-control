"""Arithmetic/inference invariants; no synthetic navigation performance claims."""
import itertools,unittest
import numpy as np
from scipy.special import logsumexp
from functional import basis, make_prior, gains, prefixes, coarse_path, matrices, refine

class FunctionalTests(unittest.TestCase):
    def test_complete_function_endpoints_and_slope(self):
        self.assertLess(np.max(np.abs(basis(np.array([0.,1.]),12))),1e-28)
        h=1e-7
        self.assertLess(np.max(np.abs(basis(np.array([h,1-h]),12))/h),1.1e-6)

    def test_integral_matches_dense_gaussian(self):
        n=64; phi=basis((np.arange(n)+.5)/n,3)
        mu=np.array([2.,-.5,.25]); cov=np.array([[2.,.2,.1],[.2,1.,-.1],[.1,-.1,.7]])
        y=np.sin(np.arange(n)*.41); weight=np.linspace(.5,1.5,n)
        prior=make_prior(mu,cov)
        got=gains(0,n,prefixes(y,weight),prior,False)[0]
        dense=np.diag(1/weight)+phi@cov@phi.T
        err=y-phi@mu
        logp=-.5*(np.linalg.slogdet(dense)[1]+err@np.linalg.solve(dense,err)+n*np.log(2*np.pi))
        base=-.5*(np.sum(y*y*weight)-np.sum(np.log(weight))+n*np.log(2*np.pi))
        self.assertAlmostEqual(got,logp-base,places=10)

    def test_joint_path_matches_exhaustive_partitions(self):
        n=16; y=np.array([0.,1.,2.,0.]*4)
        model={'mm':make_prior(np.array([4.,1.,0.]),np.eye(3)*3),
               'bl':make_prior(np.zeros(3),np.eye(3))}
        pref=prefixes(y,np.ones(n)); grid,cost=matrices(n,pref,model)
        best=-np.inf
        for bits in itertools.product([0,1],repeat=len(grid)-2):
            bounds=[0]+[i+1 for i,x in enumerate(bits) if x]+[len(grid)-1]
            for startstate in (0,1):
                score=0.
                for j,(a,b) in enumerate(zip(bounds[:-1],bounds[1:])):
                    state=(startstate+j)%2
                    score+=cost[state][a,b]-(np.log(n) if state else 0.)
                best=max(best,score)
        path,score=coarse_path(n,pref,model)
        self.assertAlmostEqual(best,score,places=10)
        fine=refine(path,n,pref,model)
        self.assertEqual(fine[0][1],0); self.assertEqual(fine[-1][2],n)
        for a,b in zip(fine[:-1],fine[1:]):
            self.assertEqual(a[2],b[1]); self.assertNotEqual(a[0],b[0])
        self.assertTrue(all(b>a for _,a,b in fine))

if __name__=='__main__': unittest.main()
