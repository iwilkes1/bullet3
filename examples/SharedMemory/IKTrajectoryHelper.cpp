#include "IKTrajectoryHelper.h"
#include "BussIK/Node.h"
#include "BussIK/Tree.h"
#include "BussIK/Jacobian.h"
#include "BussIK/VectorRn.h"
#include "BussIK/MatrixRmn.h"
#include "Bullet3Common/b3AlignedObjectArray.h"
#include "BulletDynamics/Featherstone/btMultiBody.h"


#define RADIAN(X)	((X)*RadiansToDegrees)

//use BussIK and Reflexxes to convert from Cartesian endeffector future target to
//joint space positions at each real-time (simulation) step
struct IKTrajectoryHelperInternalData
{
    VectorR3 m_endEffectorTargetPosition;
    
    
    b3AlignedObjectArray<Node*> m_ikNodes;
    Jacobian* m_ikJacobian;
    
    IKTrajectoryHelperInternalData()
    {
        m_endEffectorTargetPosition.SetZero();
    }
};


IKTrajectoryHelper::IKTrajectoryHelper()
{
    m_data = new IKTrajectoryHelperInternalData;
}

IKTrajectoryHelper::~IKTrajectoryHelper()
{
    delete m_data;
}


bool IKTrajectoryHelper::computeIK(const double endEffectorTargetPosition[3],
                                   const double endEffectorTargetOrientation[4],
                                   const double endEffectorWorldPosition[3],
                                   const double endEffectorWorldOrientation[4],
               const double* q_current, int numQ,int endEffectorIndex,
               double* q_new, int ikMethod, const double* linear_jacobian, const double* angular_jacobian, int jacobian_size, const double dampIk[6])
{
	bool useAngularPart = (ikMethod==IK2_VEL_DLS_WITH_ORIENTATION) ? true : false;

	m_data->m_ikJacobian = new Jacobian(useAngularPart,numQ);
	
//    Reset(m_ikTree,m_ikJacobian);

    m_data->m_ikJacobian->Reset();

    bool UseJacobianTargets1 = false;
    
    if ( UseJacobianTargets1 ) {
        m_data->m_ikJacobian->SetJtargetActive();
    }
    else {
        m_data->m_ikJacobian->SetJendActive();
    }
    VectorR3 targets;
    targets.Set(endEffectorTargetPosition[0],endEffectorTargetPosition[1],endEffectorTargetPosition[2]);
    m_data->m_ikJacobian->ComputeJacobian(&targets);						// Set up Jacobian and deltaS vectors
    
    // Set one end effector world position from Bullet
    VectorRn deltaS(3);
    for (int i = 0; i < 3; ++i)
    {
        deltaS.Set(i,dampIk[i]*(endEffectorTargetPosition[i]-endEffectorWorldPosition[i]));
    }
    
    // Set one end effector world orientation from Bullet
    VectorRn deltaR(3);
    btQuaternion startQ(endEffectorWorldOrientation[0],endEffectorWorldOrientation[1],endEffectorWorldOrientation[2],endEffectorWorldOrientation[3]);
    btQuaternion endQ(endEffectorTargetOrientation[0],endEffectorTargetOrientation[1],endEffectorTargetOrientation[2],endEffectorTargetOrientation[3]);
    btQuaternion deltaQ = endQ*startQ.inverse();
    float angle = deltaQ.getAngle();
    btVector3 axis = deltaQ.getAxis();
    float angleDot = angle;
    btVector3 angularVel = angleDot*axis.normalize();
    for (int i = 0; i < 3; ++i)
    {
        deltaR.Set(i,dampIk[i+3]*angularVel[i]);
    }
    
    {
        
		if (useAngularPart)
		{
			VectorRn deltaC(6);
			MatrixRmn completeJacobian(6,numQ);
			for (int i = 0; i < 3; ++i)
			{
				deltaC.Set(i,deltaS[i]);
				deltaC.Set(i+3,deltaR[i]);
				for (int j = 0; j < numQ; ++j)
				{
					completeJacobian.Set(i,j,linear_jacobian[i*numQ+j]);
					completeJacobian.Set(i+3,j,angular_jacobian[i*numQ+j]);
				}
			}
			m_data->m_ikJacobian->SetDeltaS(deltaC);
			m_data->m_ikJacobian->SetJendTrans(completeJacobian);
		} else
		{
			VectorRn deltaC(3);
			MatrixRmn completeJacobian(3,numQ);
			for (int i = 0; i < 3; ++i)
			{
				deltaC.Set(i,deltaS[i]);
				for (int j = 0; j < numQ; ++j)
				{
					completeJacobian.Set(i,j,linear_jacobian[i*numQ+j]);
				}
			}
			m_data->m_ikJacobian->SetDeltaS(deltaC);
			m_data->m_ikJacobian->SetJendTrans(completeJacobian);
		}
    }
    
    // Calculate the change in theta values
    switch (ikMethod) {
        case IK2_JACOB_TRANS:
            m_data->m_ikJacobian->CalcDeltaThetasTranspose();		// Jacobian transpose method
            break;
		case IK2_DLS:
        case IK2_VEL_DLS:
		case IK2_VEL_DLS_WITH_ORIENTATION:
            m_data->m_ikJacobian->CalcDeltaThetasDLS();			// Damped least squares method
            break;
        case IK2_DLS_SVD:
            m_data->m_ikJacobian->CalcDeltaThetasDLSwithSVD();
            break;
        case IK2_PURE_PSEUDO:
            m_data->m_ikJacobian->CalcDeltaThetasPseudoinverse();	// Pure pseudoinverse method
            break;
        case IK2_SDLS:
            m_data->m_ikJacobian->CalcDeltaThetasSDLS();			// Selectively damped least squares method
            break;
        default:
            m_data->m_ikJacobian->ZeroDeltaThetas();
            break;
    }
    
    // Use for velocity IK, update theta dot
    //m_data->m_ikJacobian->UpdateThetaDot();
    
    // Use for position IK, incrementally update theta
    //m_data->m_ikJacobian->UpdateThetas();
    
    // Apply the change in the theta values
    //m_data->m_ikJacobian->UpdatedSClampValue(&targets);
    
    for (int i=0;i<numQ;i++)
    {
        // Use for velocity IK
        q_new[i] = m_data->m_ikJacobian->dTheta[i] + q_current[i];
        
        // Use for position IK
        //q_new[i] = m_data->m_ikNodes[i]->GetTheta();
    }
    return true;
}
