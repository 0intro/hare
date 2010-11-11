
c---------------------------------------------------------------------
c---------------------------------------------------------------------

      subroutine jacld(l, np, indxp, jndxp)

c---------------------------------------------------------------------
c---------------------------------------------------------------------


c---------------------------------------------------------------------
c   compute the lower triangular part of the jacobian matrix
c---------------------------------------------------------------------

      implicit none

      include 'applu.incl'

c---------------------------------------------------------------------
c  input parameters
c---------------------------------------------------------------------
      integer l, np, indxp(*), jndxp(*)

c---------------------------------------------------------------------
c  local variables
c---------------------------------------------------------------------
      integer i, j, k, n
      double precision  r43
      double precision  c1345
      double precision  c34
      double precision  tmp1, tmp2, tmp3



!$OMP PARALLEL DEFAULT(SHARED) PRIVATE(tmp3,tmp2,tmp1,k,i,j,n,c34,c1345,
!$OMP& r43)
!$OMP&  SHARED(tx2,ty2,tz2,dz5,dy5,dx5,dz4,dy4,dx4,dz3,dy3,dx3,dz2,dy2,
!$OMP& dx2,dz1,tz1,dy1,ty1,dx1,tx1,dt,l,np)
      r43 = ( 4.0d+00 / 3.0d+00 )
      c1345 = c1 * c3 * c4 * c5
      c34 = c3 * c4

!$OMP DO
      do n = 1, np

         j = jndxp(n)
         i = indxp(n)
         k = l - i - j

c---------------------------------------------------------------------
c   form the block daigonal
c---------------------------------------------------------------------
               tmp1 = rho_i(i,j,k)
               tmp2 = tmp1 * tmp1
               tmp3 = tmp1 * tmp2

               d(1,1,n) =  1.0d+00
     >                       + dt * 2.0d+00 * (   tx1 * dx1
     >                                          + ty1 * dy1
     >                                          + tz1 * dz1 )
               d(1,2,n) =  0.0d+00
               d(1,3,n) =  0.0d+00
               d(1,4,n) =  0.0d+00
               d(1,5,n) =  0.0d+00

               d(2,1,n) = -dt * 2.0d+00
     >          * (  tx1 * r43 + ty1 + tz1  )
     >          * c34 * tmp2 * u(2,i,j,k)
               d(2,2,n) =  1.0d+00
     >          + dt * 2.0d+00 * c34 * tmp1 
     >          * (  tx1 * r43 + ty1 + tz1 )
     >          + dt * 2.0d+00 * (   tx1 * dx2
     >                             + ty1 * dy2
     >                             + tz1 * dz2  )
               d(2,3,n) = 0.0d+00
               d(2,4,n) = 0.0d+00
               d(2,5,n) = 0.0d+00

               d(3,1,n) = -dt * 2.0d+00
     >           * (  tx1 + ty1 * r43 + tz1  )
     >           * c34 * tmp2 * u(3,i,j,k)
               d(3,2,n) = 0.0d+00
               d(3,3,n) = 1.0d+00
     >         + dt * 2.0d+00 * c34 * tmp1
     >              * (  tx1 + ty1 * r43 + tz1 )
     >         + dt * 2.0d+00 * (  tx1 * dx3
     >                           + ty1 * dy3
     >                           + tz1 * dz3 )
               d(3,4,n) = 0.0d+00
               d(3,5,n) = 0.0d+00

               d(4,1,n) = -dt * 2.0d+00
     >           * (  tx1 + ty1 + tz1 * r43  )
     >           * c34 * tmp2 * u(4,i,j,k)
               d(4,2,n) = 0.0d+00
               d(4,3,n) = 0.0d+00
               d(4,4,n) = 1.0d+00
     >         + dt * 2.0d+00 * c34 * tmp1
     >              * (  tx1 + ty1 + tz1 * r43 )
     >         + dt * 2.0d+00 * (  tx1 * dx4
     >                           + ty1 * dy4
     >                           + tz1 * dz4 )
               d(4,5,n) = 0.0d+00

               d(5,1,n) = -dt * 2.0d+00
     >  * ( ( ( tx1 * ( r43*c34 - c1345 )
     >     + ty1 * ( c34 - c1345 )
     >     + tz1 * ( c34 - c1345 ) ) * ( u(2,i,j,k) ** 2 )
     >   + ( tx1 * ( c34 - c1345 )
     >     + ty1 * ( r43*c34 - c1345 )
     >     + tz1 * ( c34 - c1345 ) ) * ( u(3,i,j,k) ** 2 )
     >   + ( tx1 * ( c34 - c1345 )
     >     + ty1 * ( c34 - c1345 )
     >     + tz1 * ( r43*c34 - c1345 ) ) * ( u(4,i,j,k) ** 2 )
     >      ) * tmp3
     >   + ( tx1 + ty1 + tz1 ) * c1345 * tmp2 * u(5,i,j,k) )

               d(5,2,n) = dt * 2.0d+00 * tmp2 * u(2,i,j,k)
     > * ( tx1 * ( r43*c34 - c1345 )
     >   + ty1 * (     c34 - c1345 )
     >   + tz1 * (     c34 - c1345 ) )
               d(5,3,n) = dt * 2.0d+00 * tmp2 * u(3,i,j,k)
     > * ( tx1 * ( c34 - c1345 )
     >   + ty1 * ( r43*c34 -c1345 )
     >   + tz1 * ( c34 - c1345 ) )
               d(5,4,n) = dt * 2.0d+00 * tmp2 * u(4,i,j,k)
     > * ( tx1 * ( c34 - c1345 )
     >   + ty1 * ( c34 - c1345 )
     >   + tz1 * ( r43*c34 - c1345 ) )
               d(5,5,n) = 1.0d+00
     >   + dt * 2.0d+00 * ( tx1  + ty1 + tz1 ) * c1345 * tmp1
     >   + dt * 2.0d+00 * (  tx1 * dx5
     >                    +  ty1 * dy5
     >                    +  tz1 * dz5 )

c---------------------------------------------------------------------
c   form the first block sub-diagonal
c---------------------------------------------------------------------
               tmp1 = rho_i(i,j,k-1)
               tmp2 = tmp1 * tmp1
               tmp3 = tmp1 * tmp2

               a(1,1,n) = - dt * tz1 * dz1
               a(1,2,n) =   0.0d+00
               a(1,3,n) =   0.0d+00
               a(1,4,n) = - dt * tz2
               a(1,5,n) =   0.0d+00

               a(2,1,n) = - dt * tz2
     >           * ( - ( u(2,i,j,k-1)*u(4,i,j,k-1) ) * tmp2 )
     >           - dt * tz1 * ( - c34 * tmp2 * u(2,i,j,k-1) )
               a(2,2,n) = - dt * tz2 * ( u(4,i,j,k-1) * tmp1 )
     >           - dt * tz1 * c34 * tmp1
     >           - dt * tz1 * dz2 
               a(2,3,n) = 0.0d+00
               a(2,4,n) = - dt * tz2 * ( u(2,i,j,k-1) * tmp1 )
               a(2,5,n) = 0.0d+00

               a(3,1,n) = - dt * tz2
     >           * ( - ( u(3,i,j,k-1)*u(4,i,j,k-1) ) * tmp2 )
     >           - dt * tz1 * ( - c34 * tmp2 * u(3,i,j,k-1) )
               a(3,2,n) = 0.0d+00
               a(3,3,n) = - dt * tz2 * ( u(4,i,j,k-1) * tmp1 )
     >           - dt * tz1 * ( c34 * tmp1 )
     >           - dt * tz1 * dz3
               a(3,4,n) = - dt * tz2 * ( u(3,i,j,k-1) * tmp1 )
               a(3,5,n) = 0.0d+00

               a(4,1,n) = - dt * tz2
     >        * ( - ( u(4,i,j,k-1) * tmp1 ) ** 2
     >            + c2 * qs(i,j,k-1) * tmp1 )
     >        - dt * tz1 * ( - r43 * c34 * tmp2 * u(4,i,j,k-1) )
               a(4,2,n) = - dt * tz2
     >             * ( - c2 * ( u(2,i,j,k-1) * tmp1 ) )
               a(4,3,n) = - dt * tz2
     >             * ( - c2 * ( u(3,i,j,k-1) * tmp1 ) )
               a(4,4,n) = - dt * tz2 * ( 2.0d+00 - c2 )
     >             * ( u(4,i,j,k-1) * tmp1 )
     >             - dt * tz1 * ( r43 * c34 * tmp1 )
     >             - dt * tz1 * dz4
               a(4,5,n) = - dt * tz2 * c2

               a(5,1,n) = - dt * tz2
     >       * ( ( c2 * 2.0d0 * qs(i,j,k-1)
     >       - c1 * u(5,i,j,k-1) )
     >            * u(4,i,j,k-1) * tmp2 )
     >       - dt * tz1
     >       * ( - ( c34 - c1345 ) * tmp3 * (u(2,i,j,k-1)**2)
     >           - ( c34 - c1345 ) * tmp3 * (u(3,i,j,k-1)**2)
     >           - ( r43*c34 - c1345 )* tmp3 * (u(4,i,j,k-1)**2)
     >          - c1345 * tmp2 * u(5,i,j,k-1) )
               a(5,2,n) = - dt * tz2
     >       * ( - c2 * ( u(2,i,j,k-1)*u(4,i,j,k-1) ) * tmp2 )
     >       - dt * tz1 * ( c34 - c1345 ) * tmp2 * u(2,i,j,k-1)
               a(5,3,n) = - dt * tz2
     >       * ( - c2 * ( u(3,i,j,k-1)*u(4,i,j,k-1) ) * tmp2 )
     >       - dt * tz1 * ( c34 - c1345 ) * tmp2 * u(3,i,j,k-1)
               a(5,4,n) = - dt * tz2
     >       * ( c1 * ( u(5,i,j,k-1) * tmp1 )
     >       - c2
     >       * ( qs(i,j,k-1) * tmp1
     >            + u(4,i,j,k-1)*u(4,i,j,k-1) * tmp2 ) )
     >       - dt * tz1 * ( r43*c34 - c1345 ) * tmp2 * u(4,i,j,k-1)
               a(5,5,n) = - dt * tz2
     >       * ( c1 * ( u(4,i,j,k-1) * tmp1 ) )
     >       - dt * tz1 * c1345 * tmp1
     >       - dt * tz1 * dz5

c---------------------------------------------------------------------
c   form the second block sub-diagonal
c---------------------------------------------------------------------
               tmp1 = rho_i(i,j-1,k)
               tmp2 = tmp1 * tmp1
               tmp3 = tmp1 * tmp2

               b(1,1,n) = - dt * ty1 * dy1
               b(1,2,n) =   0.0d+00
               b(1,3,n) = - dt * ty2
               b(1,4,n) =   0.0d+00
               b(1,5,n) =   0.0d+00

               b(2,1,n) = - dt * ty2
     >           * ( - ( u(2,i,j-1,k)*u(3,i,j-1,k) ) * tmp2 )
     >           - dt * ty1 * ( - c34 * tmp2 * u(2,i,j-1,k) )
               b(2,2,n) = - dt * ty2 * ( u(3,i,j-1,k) * tmp1 )
     >          - dt * ty1 * ( c34 * tmp1 )
     >          - dt * ty1 * dy2
               b(2,3,n) = - dt * ty2 * ( u(2,i,j-1,k) * tmp1 )
               b(2,4,n) = 0.0d+00
               b(2,5,n) = 0.0d+00

               b(3,1,n) = - dt * ty2
     >           * ( - ( u(3,i,j-1,k) * tmp1 ) ** 2
     >       + c2 * ( qs(i,j-1,k) * tmp1 ) )
     >       - dt * ty1 * ( - r43 * c34 * tmp2 * u(3,i,j-1,k) )
               b(3,2,n) = - dt * ty2
     >                   * ( - c2 * ( u(2,i,j-1,k) * tmp1 ) )
               b(3,3,n) = - dt * ty2 * ( ( 2.0d+00 - c2 )
     >                   * ( u(3,i,j-1,k) * tmp1 ) )
     >       - dt * ty1 * ( r43 * c34 * tmp1 )
     >       - dt * ty1 * dy3
               b(3,4,n) = - dt * ty2
     >                   * ( - c2 * ( u(4,i,j-1,k) * tmp1 ) )
               b(3,5,n) = - dt * ty2 * c2

               b(4,1,n) = - dt * ty2
     >              * ( - ( u(3,i,j-1,k)*u(4,i,j-1,k) ) * tmp2 )
     >       - dt * ty1 * ( - c34 * tmp2 * u(4,i,j-1,k) )
               b(4,2,n) = 0.0d+00
               b(4,3,n) = - dt * ty2 * ( u(4,i,j-1,k) * tmp1 )
               b(4,4,n) = - dt * ty2 * ( u(3,i,j-1,k) * tmp1 )
     >                        - dt * ty1 * ( c34 * tmp1 )
     >                        - dt * ty1 * dy4
               b(4,5,n) = 0.0d+00

               b(5,1,n) = - dt * ty2
     >          * ( ( c2 * 2.0d0 * qs(i,j-1,k)
     >               - c1 * u(5,i,j-1,k) )
     >          * ( u(3,i,j-1,k) * tmp2 ) )
     >          - dt * ty1
     >          * ( - (     c34 - c1345 )*tmp3*(u(2,i,j-1,k)**2)
     >              - ( r43*c34 - c1345 )*tmp3*(u(3,i,j-1,k)**2)
     >              - (     c34 - c1345 )*tmp3*(u(4,i,j-1,k)**2)
     >              - c1345*tmp2*u(5,i,j-1,k) )
               b(5,2,n) = - dt * ty2
     >          * ( - c2 * ( u(2,i,j-1,k)*u(3,i,j-1,k) ) * tmp2 )
     >          - dt * ty1
     >          * ( c34 - c1345 ) * tmp2 * u(2,i,j-1,k)
               b(5,3,n) = - dt * ty2
     >          * ( c1 * ( u(5,i,j-1,k) * tmp1 )
     >          - c2 
     >          * ( qs(i,j-1,k) * tmp1
     >               + u(3,i,j-1,k)*u(3,i,j-1,k) * tmp2 ) )
     >          - dt * ty1
     >          * ( r43*c34 - c1345 ) * tmp2 * u(3,i,j-1,k)
               b(5,4,n) = - dt * ty2
     >          * ( - c2 * ( u(3,i,j-1,k)*u(4,i,j-1,k) ) * tmp2 )
     >          - dt * ty1 * ( c34 - c1345 ) * tmp2 * u(4,i,j-1,k)
               b(5,5,n) = - dt * ty2
     >          * ( c1 * ( u(3,i,j-1,k) * tmp1 ) )
     >          - dt * ty1 * c1345 * tmp1
     >          - dt * ty1 * dy5

c---------------------------------------------------------------------
c   form the third block sub-diagonal
c---------------------------------------------------------------------
               tmp1 = rho_i(i-1,j,k)
               tmp2 = tmp1 * tmp1
               tmp3 = tmp1 * tmp2

               c(1,1,n) = - dt * tx1 * dx1
               c(1,2,n) = - dt * tx2
               c(1,3,n) =   0.0d+00
               c(1,4,n) =   0.0d+00
               c(1,5,n) =   0.0d+00

               c(2,1,n) = - dt * tx2
     >          * ( - ( u(2,i-1,j,k) * tmp1 ) ** 2
     >       + c2 * qs(i-1,j,k) * tmp1 )
     >          - dt * tx1 * ( - r43 * c34 * tmp2 * u(2,i-1,j,k) )
               c(2,2,n) = - dt * tx2
     >          * ( ( 2.0d+00 - c2 ) * ( u(2,i-1,j,k) * tmp1 ) )
     >          - dt * tx1 * ( r43 * c34 * tmp1 )
     >          - dt * tx1 * dx2
               c(2,3,n) = - dt * tx2
     >              * ( - c2 * ( u(3,i-1,j,k) * tmp1 ) )
               c(2,4,n) = - dt * tx2
     >              * ( - c2 * ( u(4,i-1,j,k) * tmp1 ) )
               c(2,5,n) = - dt * tx2 * c2 

               c(3,1,n) = - dt * tx2
     >              * ( - ( u(2,i-1,j,k) * u(3,i-1,j,k) ) * tmp2 )
     >         - dt * tx1 * ( - c34 * tmp2 * u(3,i-1,j,k) )
               c(3,2,n) = - dt * tx2 * ( u(3,i-1,j,k) * tmp1 )
               c(3,3,n) = - dt * tx2 * ( u(2,i-1,j,k) * tmp1 )
     >          - dt * tx1 * ( c34 * tmp1 )
     >          - dt * tx1 * dx3
               c(3,4,n) = 0.0d+00
               c(3,5,n) = 0.0d+00

               c(4,1,n) = - dt * tx2
     >          * ( - ( u(2,i-1,j,k)*u(4,i-1,j,k) ) * tmp2 )
     >          - dt * tx1 * ( - c34 * tmp2 * u(4,i-1,j,k) )
               c(4,2,n) = - dt * tx2 * ( u(4,i-1,j,k) * tmp1 )
               c(4,3,n) = 0.0d+00
               c(4,4,n) = - dt * tx2 * ( u(2,i-1,j,k) * tmp1 )
     >          - dt * tx1 * ( c34 * tmp1 )
     >          - dt * tx1 * dx4
               c(4,5,n) = 0.0d+00

               c(5,1,n) = - dt * tx2
     >          * ( ( c2 * 2.0d0 * qs(i-1,j,k)
     >              - c1 * u(5,i-1,j,k) )
     >          * u(2,i-1,j,k) * tmp2 )
     >          - dt * tx1
     >          * ( - ( r43*c34 - c1345 ) * tmp3 * ( u(2,i-1,j,k)**2 )
     >              - (     c34 - c1345 ) * tmp3 * ( u(3,i-1,j,k)**2 )
     >              - (     c34 - c1345 ) * tmp3 * ( u(4,i-1,j,k)**2 )
     >              - c1345 * tmp2 * u(5,i-1,j,k) )
               c(5,2,n) = - dt * tx2
     >          * ( c1 * ( u(5,i-1,j,k) * tmp1 )
     >             - c2
     >             * ( u(2,i-1,j,k)*u(2,i-1,j,k) * tmp2
     >                  + qs(i-1,j,k) * tmp1 ) )
     >           - dt * tx1
     >           * ( r43*c34 - c1345 ) * tmp2 * u(2,i-1,j,k)
               c(5,3,n) = - dt * tx2
     >           * ( - c2 * ( u(3,i-1,j,k)*u(2,i-1,j,k) ) * tmp2 )
     >           - dt * tx1
     >           * (  c34 - c1345 ) * tmp2 * u(3,i-1,j,k)
               c(5,4,n) = - dt * tx2
     >           * ( - c2 * ( u(4,i-1,j,k)*u(2,i-1,j,k) ) * tmp2 )
     >           - dt * tx1
     >           * (  c34 - c1345 ) * tmp2 * u(4,i-1,j,k)
               c(5,5,n) = - dt * tx2
     >           * ( c1 * ( u(2,i-1,j,k) * tmp1 ) )
     >           - dt * tx1 * c1345 * tmp1
     >           - dt * tx1 * dx5

      end do
!$OMP END DO nowait
!$OMP END PARALLEL

      return
      end
