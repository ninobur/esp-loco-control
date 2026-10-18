"""Whole-function alternating-state research model. Recognition sees ADC only.

No NAVI, movement, map, timing prediction, or amplitude-admission inputs.
Interval likelihoods are factors of a joint partition, never accepted detections.
"""
import numpy as np
from scipy.special import logsumexp
from scipy.linalg import solveh_banded

Q = 64
K = 12
COARSE = 4
NU = 5.0
UPDATES = 3

def basis(u, k):
    u = np.asarray(u)
    return np.sin(np.pi*u)[..., None]**2 * np.cos(np.pi*u[..., None]*np.arange(k))

PHI = {k: basis((np.arange(Q)+.5)/Q, k) for k in (3, K)}

def make_prior(mean, cov, means=None):
    mean = np.asarray(mean); cov = np.asarray(cov)
    return dict(mean=mean.tolist(), cov=cov.tolist(), means=None if means is None else np.asarray(means).tolist())

def modes(prior, magnetic):
    mu = np.array(prior['means'] if prior.get('means') is not None else [prior['mean']])
    cov = np.array(prior['cov'])
    if magnetic:
        # Whole-function reversal transforms every coefficient and covariance.
        rev = (-1.)**np.arange(len(cov))
        return [(np.concatenate([mu, -mu]), cov),
                (np.concatenate([mu*rev, -mu*rev]), cov*rev[:,None]*rev[None,:])]
    return [(mu, cov)]

def prefixes(r, weight):
    return np.r_[0., np.cumsum(weight)], np.r_[0., np.cumsum(weight*r)]

def stats(lo, hi, pref):
    lo = np.atleast_1d(lo); hi = np.atleast_1d(hi)
    edges = lo[:,None] + (hi-lo)[:,None]*np.arange(Q+1)[None,:]/Q
    x = np.arange(len(pref[0]))
    return tuple(np.diff(np.interp(edges, x, p), axis=1) for p in pref)

def gains(lo, hi, pref, prior, magnetic, posterior=False):
    W, Y = stats(lo, hi, pref)
    k = len(prior['mean']); phi = PHI[k]
    gram = np.einsum('bq,qj,ql->bjl', W, phi, phi, optimize=True)
    rhs = Y @ phi
    terms=[]; post=[]
    for means, cov in modes(prior, magnetic):
        inv = np.linalg.inv(cov)
        precision = gram + inv
        # Batched inverse is reused for all geographic means.
        S = np.linalg.inv(precision)
        q = rhs[:,None,:] + (means @ inv)[None,:,:]
        m = np.einsum('bij,bcj->bci', S, q)
        val = .5*np.einsum('bci,bci->bc', q, m) - .5*np.einsum('ci,ij,cj->c', means, inv, means)[None,:]
        val -= .5*(np.linalg.slogdet(cov)[1]+np.linalg.slogdet(precision)[1])[:,None]
        terms.append(val)
        if posterior: post.append((m,S))
    values = np.concatenate(terms,axis=1)
    score = logsumexp(values,axis=1)-np.log(values.shape[1])
    if not posterior: return score
    idx = np.argmax(values[0]); group=0
    while idx >= post[group][0].shape[1]:
        idx -= post[group][0].shape[1]; group += 1
    return score[0], post[group][0][0,idx], post[group][1][0]

def matrices(n, pref, model):
    grid=np.unique(np.r_[np.arange(0,n,COARSE),n]).astype(int)
    size=len(grid)
    costs=[np.full((size,size),-np.inf) for _ in range(2)]
    for end in range(1,size):
        lo=grid[:end]; hi=np.full(end,grid[end])
        for state, key in enumerate(('bl','mm')):
            costs[state][:end,end]=gains(lo,hi,pref,model[key],state==1)
    return grid,costs

def coarse_path(n,pref,model):
    grid,cost=matrices(n,pref,model); size=len(grid)
    dp=np.full((2,size),-np.inf); dp[:,0]=0.
    back=np.zeros((2,size),int); penalty=np.log(max(n,2))
    for end in range(1,size):
        for state in range(2):
            vals=dp[1-state,:end]+cost[state][:end,end]-(penalty if state else 0.)
            arg=int(np.argmax(vals)); dp[state,end]=vals[arg]; back[state,end]=arg
    state=int(np.argmax(dp[:,-1])); end=size-1; path=[]
    while end:
        start=int(back[state,end]); path.append([state,int(grid[start]),int(grid[end])])
        end=start; state=1-state
    return list(reversed(path)),float(np.max(dp[:,-1]))

def refine(path,n,pref,model):
    """Joint small-domain DP over all boundary variables, fixed coarse topology."""
    domains=[np.array([0])]+[np.arange(max(1,p[2]-COARSE),min(n,p[2]+COARSE)+1) for p in path[:-1]]+[np.array([n])]
    values=np.array([0.]); backs=[]
    for j,p in enumerate(path):
        left,right=domains[j],domains[j+1]
        a,b=np.meshgrid(left,right,indexing='ij'); good=a<b
        v=np.full(a.shape,-np.inf)
        v[good]=gains(a[good],b[good],pref,model['mm' if p[0] else 'bl'],bool(p[0]))
        total=values[:,None]+v; arg=np.argmax(total,axis=0)
        values=total[arg,np.arange(len(right))]; backs.append(arg)
    idx=0; out=[]
    for j in range(len(path)-1,-1,-1):
        prev=int(backs[j][idx]); out.append([path[j][0],int(domains[j][prev]),int(domains[j+1][idx])]); idx=prev
    return list(reversed(out))

def smooth(target, weight, strength):
    n=len(target)
    if n<3: return np.full(n,np.average(target,weights=weight))
    # Upper-band representation of diag(weight)+strength*D2.T*D2.
    d=np.full(n,6.); d[:2]=[1,5]; d[-2:]=[5,1]
    if n==3: d=np.array([1.,4.,1.])
    off=np.full(n-1,-4.); off[[0,-1]]=-2.
    ab=np.zeros((3,n)); ab[2]=weight+strength*d+1e-9
    ab[1,1:]=strength*off; ab[0,2:]=strength
    return solveh_banded(ab,weight*target,lower=False,check_finite=False)

def infer(adc,model):
    y=np.asarray(adc,float); n=len(y); sigma=model['sigma']; strength=model['background_strength']
    b=np.full(n,np.median(y)); lam=np.ones(n); path=[]; score=0.
    for iteration in range(UPDATES):
        pref=prefixes(y-b,lam/sigma**2)
        path,score=coarse_path(n,pref,model)
        path=refine(path,n,pref,model)
        f=np.zeros(n); variance=np.zeros(n); objects=[]
        for state,lo,hi in path:
            key='mm' if state else 'bl'
            _,mean,cov=gains(lo,hi,pref,model[key],bool(state),True)
            phi=basis((np.arange(hi-lo)+.5)/(hi-lo),len(mean))
            f[lo:hi]=phi@mean
            variance[lo:hi]=np.einsum('ij,jk,ik->i',phi,cov,phi)
            objects.append(dict(state=key,lo=lo,hi=hi,mean=mean,cov=cov,censored=(lo==0 or hi==n)))
        b=smooth(y-f,lam/sigma**2,strength)
        lam=(NU+1)/(NU+((y-b-f)**2+variance)/sigma**2)
    return dict(objects=objects,baseline=b,field=f,precision=lam,score=score,
                residual_rms=float(np.sqrt(np.mean((y-b-f)**2))))

def canonical(mean,cov):
    """Whole-vector nuisance orientation; never independently canonicalize lobes."""
    phi=PHI[len(mean)]; curve=phi@mean; i=np.argmax(np.abs(curve))
    s=1. if curve[i]>=0 else -1.; t=np.ones(len(mean))
    if i>Q/2: t=(-1.)**np.arange(len(mean))
    t*=s
    return mean*t,cov*t[:,None]*t[None,:]
